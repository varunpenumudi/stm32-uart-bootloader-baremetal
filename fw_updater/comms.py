import asyncio
import serial_asyncio
from enum import Enum
import sys

# Coms Packets
COMMS_PACKET_DATALEN_LEN           = 1
COMMS_PACKET_PAYLOAD_LEN           = 16
COMMS_PACKET_CRC_LEN               = 1
COMMS_PACKET_FULL_LEN              = COMMS_PACKET_DATALEN_LEN + COMMS_PACKET_PAYLOAD_LEN + COMMS_PACKET_CRC_LEN

COMMS_RETX_PACKET_DATA0            = 0x15
COMMS_ACK_PACKET_DATA0             = 0x19

# BL Packets
DEVICE_ID                         = 0x52
DEFAULT_TIMEOUT                   = 5000
SYNC_SEQ_B0                       = 0xAA
SYNC_SEQ_B1                       = 0xBB
SYNC_SEQ_B2                       = 0xCC
SYNC_SEQ_B3                       = 0xDD
SYNC_SEQ_BYTES                    = [SYNC_SEQ_B0, SYNC_SEQ_B1, SYNC_SEQ_B2, SYNC_SEQ_B3]

BL_PACKET_SEQ_OBSERVED_DATA0      = 0x23
BL_PACKET_FW_UPDATE_REQ_DATA0     = 0x25
BL_PACKET_FW_UPDATE_RES_DATA0     = 0x26
BL_PACKET_DEVICE_ID_REQ_DATA0     = 0x31
BL_PACKET_DEVICE_ID_RES_DATA0     = 0x32
BL_PACKET_FW_LENGTH_REQ_DATA0     = 0x35
BL_PACKET_FW_LENGTH_RES_DATA0     = 0x36
BL_PACKET_READY_FOR_DATA_DATA0    = 0x39
BL_PACKET_FW_UPDATE_SUCCESS_DATA0 = 0x41

DEBUG_BL = False

def crc8(buffer: list[int]) -> int:
    crc = 0
    for byte in buffer:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

def create_packet(payload: list[int]) -> bytes:
    padded_payload = payload + [0xFF] * (16 - len(payload))   # pad 0xFF's for 16 bytes
    data = [len(payload)] + padded_payload                    # data before CRC = [LEN] + payload(16)
    crc = crc8(data)                                          # compute CRC
    return bytes(data + [crc])                                # return full packet (18 bytes)


# Special Packets
ACK_PACKET = create_packet([COMMS_ACK_PACKET_DATA0])
REQ_RETX_PACKET = create_packet([COMMS_RETX_PACKET_DATA0])
recv_packets_buff: asyncio.Queue[bytes] = asyncio.Queue()
last_transmit_packet: bytes = bytes([0x00] * 18)


async def transmit_packet(transport: serial_asyncio.SerialTransport, packet: bytes):
    global last_transmit_packet
    transport.write(packet)
    last_transmit_packet = packet

    while True:
        pkt = await recv_packets_buff.get()   # wait for next received packet
        if pkt == ACK_PACKET:
            print("Ack Received")
            break
        elif pkt == REQ_RETX_PACKET:
            print("Retransmit Requested")
            transport.write(last_transmit_packet)
        # ignore everything else


class BL_STATE(Enum):
    BL_State_Sync = 0
    BL_State_SendUpdateReq = 1
    BL_State_WaitForUpdateRes = 2
    BL_State_DeviceIDReq = 3
    BL_State_DeviceIDRes = 4
    BL_State_FwLengthReq = 5
    BL_State_FwLengthRes = 6
    BL_State_EraseApplication = 7
    BL_State_RecieveFirmware = 8
    BL_State_UpdateSuccess = 9


class SerialProtocol(asyncio.Protocol):
    def __init__(self):
        self.transport = None
        self.cur_buff = bytes()

    def connection_made(self, transport):
        self.transport = transport
        print("✅ Serial port opened")

    def data_received(self, data):
        self.cur_buff += data
        while len(self.cur_buff) >= 18:
            packet = self.cur_buff[:18]
            recv_packets_buff.put_nowait(packet)
            self.cur_buff = self.cur_buff[18:]

    def connection_lost(self, exc):
        print("❌ Serial port closed")


async def bl_state_machine(transport: serial_asyncio.SerialTransport, protocol, fw_length, fw_bytes):
    state = BL_STATE.BL_State_Sync
    seq_byts = bytes([0xAA, 0xBB, 0xCC, 0xDD])
    offset = 0

    while True:
        if state != BL_STATE.BL_State_RecieveFirmware: print(f"{state}")

        match state:
            case BL_STATE.BL_State_Sync:
                transport.write(seq_byts)
                pkt = await recv_packets_buff.get()
                if pkt and pkt == create_packet([BL_PACKET_SEQ_OBSERVED_DATA0]):
                    print("[RECV-SeqObserved]:", pkt.hex(' '))
                    state = BL_STATE.BL_State_SendUpdateReq

            case BL_STATE.BL_State_SendUpdateReq:
                pkt = await recv_packets_buff.get()
                if pkt == create_packet([BL_PACKET_FW_UPDATE_REQ_DATA0]):
                    print("[RECV-UpdateReq]:", pkt.hex(' '))
                    state = BL_STATE.BL_State_WaitForUpdateRes
            
            case BL_STATE.BL_State_WaitForUpdateRes:
                pckt = create_packet([BL_PACKET_FW_UPDATE_RES_DATA0])
                await transmit_packet(transport, pckt)
                state = BL_STATE.BL_State_DeviceIDReq

            case BL_STATE.BL_State_DeviceIDReq:
                pkt = await recv_packets_buff.get()
                if pkt == create_packet([BL_PACKET_DEVICE_ID_REQ_DATA0]):
                    print("[RECV-DeviceIDReq]:", pkt.hex(' '))
                    state = BL_STATE.BL_State_DeviceIDRes

            case BL_STATE.BL_State_DeviceIDRes:
                pckt = create_packet([BL_PACKET_DEVICE_ID_RES_DATA0, DEVICE_ID])
                await transmit_packet(transport, pckt)
                state = BL_STATE.BL_State_FwLengthReq
            
            case BL_STATE.BL_State_FwLengthReq:
                pkt = await recv_packets_buff.get()
                if pkt == create_packet([BL_PACKET_FW_LENGTH_REQ_DATA0]):
                    print("[RECV-FwLengthReq]:", pkt.hex(' '))
                    state = BL_STATE.BL_State_FwLengthRes

            case BL_STATE.BL_State_FwLengthRes:
                if DEBUG_BL: input(f"{state} Start?: ")
                pckt = create_packet([
                    BL_PACKET_FW_LENGTH_RES_DATA0, 
                    (fw_length >> 24) & 0xFF, 
                    (fw_length >> 16) & 0xFF, 
                    (fw_length >> 8) & 0xFF, 
                    fw_length & 0xFF
                ])
                await transmit_packet(transport, pckt)
                state = BL_STATE.BL_State_EraseApplication
                
            case BL_STATE.BL_State_EraseApplication:
                pkt = await recv_packets_buff.get()
                if pkt == create_packet([BL_PACKET_READY_FOR_DATA_DATA0]):
                    print("[RECV-FwReadyData]:", pkt.hex(' '))
                    state = BL_STATE.BL_State_RecieveFirmware

            case BL_STATE.BL_State_RecieveFirmware:
                if DEBUG_BL:
                    input(f"{state} Start?: ")

                # Send firmware in 16-byte chunks
                chunk = fw_bytes[offset:offset+16]
                pckt = create_packet(list(chunk))
                await transmit_packet(transport, pckt)
                offset += 16

                if offset < fw_length:
                    # Get Ready for Next Packet
                    recv_pkt = await recv_packets_buff.get()
                    if recv_pkt == create_packet([BL_PACKET_READY_FOR_DATA_DATA0]):
                        # print(f"[Recv-ReadyForData]: {recv_pkt.hex(' ')}")
                        print("Bytes Remaining to Send: ", fw_length-offset)
                    else:
                        print("❌ Firmware update failed (unexpected response)")
                        return
                else:
                    state = BL_STATE.BL_State_UpdateSuccess
            
            case BL_STATE.BL_State_UpdateSuccess:
                recv_pkt = await recv_packets_buff.get()
                if recv_pkt == create_packet([BL_PACKET_FW_UPDATE_SUCCESS_DATA0]):
                    print("✅ Firmware update completed")
                    return



async def main():
    # Com Port, Baud Rate
    COM_PORT = "/dev/ttyUSB0"
    BAUD_RATE = 115200

    # Firmware Bytes, Length
    with open("../app/firmware.bin", "rb") as file:
        FW_BYTES = file.read()
    FW_LENGTH = len(FW_BYTES)

    # Run Recieve machine
    loop = asyncio.get_running_loop()
    transport, protocol = await serial_asyncio.create_serial_connection(
        loop, SerialProtocol, COM_PORT, baudrate=BAUD_RATE
    )

    # Run state machine
    await bl_state_machine(transport, protocol, FW_LENGTH, FW_BYTES)

if __name__ == "__main__":
    asyncio.run(main())
