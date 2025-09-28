#include "comms.h"
#include "core/uart.h"
#include "core/crc8.h"

#define COMMS_RECV_PACKET_BUFFER_SIZE (16U)

typedef enum {
    CommsPacket_State_DataLen_En,
    CommsPacket_State_Payload_En,
    CommsPacket_State_CRC_En,
} comms_state_t;

static comms_state_t cur_state = CommsPacket_State_DataLen_En;
static comms_packet_t cur_packet = {.length=0U, .data={0U}, .crc=0U};
static uint8_t data_bytes_read = 0;

static comms_packet_t last_trasmit_packet = {.length=0U, .data={0U}, .crc=0U};
static comms_packet_t recv_packet_buffer[COMMS_RECV_PACKET_BUFFER_SIZE] = {{0U}, {0U}, {0U}, {0U}};
static uint8_t packet_buffer_read_index = 0U;
static uint8_t packet_buffer_write_index = 0U;

static comms_packet_t retx_packet = {.length=0U, .data={0U}, .crc=0U};
static comms_packet_t ack_packet = {.length=0U, .data={0U}, .crc=0U};



void comms_setup(void) {
    comms_create_single_byte_packet(&retx_packet, COMMS_RETX_PACKET_DATA0);
    comms_create_single_byte_packet(&ack_packet, COMMS_ACK_PACKET_DATA0);
    comms_create_single_byte_packet(&last_trasmit_packet, COMMS_ACK_PACKET_DATA0);
}


void comms_update(void) {
    switch (cur_state) {
        case CommsPacket_State_DataLen_En: {
            cur_packet.length = uart_read_byte();
            cur_state = CommsPacket_State_Payload_En;
        } break;

        case CommsPacket_State_Payload_En: {
            cur_packet.data[data_bytes_read] = uart_read_byte();
            data_bytes_read++;
            if (data_bytes_read == COMMS_PACKET_PAYLOAD_LEN) {
                cur_state = CommsPacket_State_CRC_En;
            }
        } break;

        case CommsPacket_State_CRC_En: {
            uint8_t computed_crc;
            cur_packet.crc = uart_read_byte();
            computed_crc = crc8((uint8_t *) &cur_packet, COMMS_PACKET_FULL_LEN-COMMS_PACKET_CRC_LEN);
            if (cur_packet.crc != computed_crc) {
                /* Request Retransmit */
                comms_write(&retx_packet);
            }
            else if (comms_is_single_byte_packet(&cur_packet, COMMS_RETX_PACKET_DATA0)) {
                /* Got Retx Request Packet */ 
                comms_write(&last_trasmit_packet);
            }
            else if (comms_is_single_byte_packet(&cur_packet, COMMS_ACK_PACKET_DATA0)) {
                /* Got Acknowledgement Packet */ 
            }
            else {
                /* Normal Packet Recieved */
                uint8_t next_packet_buffer_write_index = (packet_buffer_write_index + 1) % COMMS_RECV_PACKET_BUFFER_SIZE;
                if (next_packet_buffer_write_index == packet_buffer_read_index)  {
                    packet_buffer_read_index = (packet_buffer_read_index + 1) % COMMS_RECV_PACKET_BUFFER_SIZE;
                }

                comms_packet_copy(&cur_packet, &recv_packet_buffer[packet_buffer_write_index]);
                packet_buffer_write_index = next_packet_buffer_write_index;

                /* Give Acknowledgement */
                comms_write(&ack_packet);
            }

            /* Reset Data Bytes, State */
            data_bytes_read = 0;
            cur_state = CommsPacket_State_DataLen_En;

        } break;

        default:  {
            cur_state = CommsPacket_State_DataLen_En;
        } break;
    }
}


bool comms_packets_available(void) {
    return (packet_buffer_read_index != packet_buffer_write_index);
}


void comms_write(comms_packet_t* packet) {
    uart_write((uint8_t *) packet, COMMS_PACKET_FULL_LEN);
    if (comms_is_single_byte_packet(packet, COMMS_RETX_PACKET_DATA0) || comms_is_single_byte_packet(packet, COMMS_ACK_PACKET_DATA0)) {
        return;
    }
    comms_packet_copy(packet, &last_trasmit_packet);
}


void comms_read(comms_packet_t* packet) {
    if (packet_buffer_read_index == packet_buffer_write_index) {
        return;
    }

    comms_packet_copy(&recv_packet_buffer[packet_buffer_read_index], packet);
    packet_buffer_read_index = (packet_buffer_read_index+1) % COMMS_RECV_PACKET_BUFFER_SIZE;
}


/* Comms Utils */
void comms_packet_copy(const comms_packet_t* source, comms_packet_t* dest) {
    dest->length = source->length;
    for (uint8_t i = 0; i < COMMS_PACKET_PAYLOAD_LEN; i++) {
        dest->data[i] = source->data[i];
    }
    dest->crc = source->crc;
}

bool comms_is_single_byte_packet(const comms_packet_t* packet, uint8_t byte)  {
    if (packet->length != 1U) {
        return false;
    }
    if (packet->data[0] != byte) {
        return false;
    }
    for (uint8_t i = 1; i < COMMS_PACKET_PAYLOAD_LEN; i++) {
        if (packet->data[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

void comms_create_single_byte_packet(comms_packet_t* packet, uint8_t byte) {
    packet->length = 0x01;
    packet->data[0] = byte;
    for (uint8_t i = 1; i < 16; i++) {
        packet->data[i] = 0xFF;
    }
    packet->crc = crc8((uint8_t *) packet, COMMS_PACKET_FULL_LEN - COMMS_PACKET_CRC_LEN);
}