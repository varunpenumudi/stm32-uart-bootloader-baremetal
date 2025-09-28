#ifndef INC_COMMS_H
#define INC_COMMS_H

#include "common-defines.h"


#define COMMS_PACKET_DATALEN_LEN (1U)
#define COMMS_PACKET_PAYLOAD_LEN (16U)
#define COMMS_PACKET_CRC_LEN     (1U)
#define COMMS_PACKET_FULL_LEN    (COMMS_PACKET_DATALEN_LEN + COMMS_PACKET_PAYLOAD_LEN + COMMS_PACKET_CRC_LEN)

#define COMMS_RETX_PACKET_DATA0 (0x15U)
#define COMMS_ACK_PACKET_DATA0  (0x19U)

#define BL_PACKET_SEQ_OBSERVED_DATA0            (0x23U)
#define BL_PACKET_FW_UPDATE_REQ_DATA0           (0x25U)
#define BL_PACKET_FW_UPDATE_RES_DATA0           (0x26U)
#define BL_PACKET_DEVICE_ID_REQ_DATA0           (0x31U)
#define BL_PACKET_DEVICE_ID_RES_DATA0           (0x32U)
#define BL_PACKET_FW_LENGTH_REQ_DATA0           (0x35U)
#define BL_PACKET_FW_LENGTH_RES_DATA0           (0x36U)
#define BL_PACKET_READY_FOR_DATA_DATA0          (0x39U)
#define BL_PACKET_FW_UPDATE_SUCCESS_DATA0       (0x41U)
#define BL_PACKET_FW_UPDATE_FAILED_DATA0        (0x42U)

typedef struct {
    uint8_t length;
    uint8_t data[COMMS_PACKET_PAYLOAD_LEN];
    uint8_t crc;
} comms_packet_t;


void comms_setup(void);
void comms_update(void);

bool comms_packets_available(void);
void comms_write(comms_packet_t* packet);
void comms_read(comms_packet_t* packet);

/* Comms Utils */
void comms_packet_copy(const comms_packet_t* source, comms_packet_t* dest);
bool comms_is_single_byte_packet(const comms_packet_t* packet, uint8_t byte);
void comms_create_single_byte_packet(comms_packet_t* packet, uint8_t byte);


#endif /* INC_COMMS_H */
