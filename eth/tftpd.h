
#ifndef __TFTPD_H__
#define __TFTPD_H__

#include "uipopt.h"

#pragma pack(1)
typedef struct {
    uint16_t opcode;
    uint8_t data[];
} TFTP_PKT;

typedef struct {
    uint16_t opcode;
    uint16_t ackn;
} TFTP_ACK_PKT;

typedef struct {
    uint16_t opcode;
    uint16_t ackn;
    uint8_t data[];
} TFTP_DATA_PKT;

typedef struct {
    uint16_t opcode;
    uint16_t e_code;
    char    e_msg[];
} TFTP_ERR_PKT;

struct tftpd_state {
    uint16_t ackn;
    uint16_t mode;
    uint32_t start, end;
    uint32_t crc;
    struct uip_udp_conn *conn;
};

void tftpd_init(void);
void tftpd_appcall(void);
void tftpd_cb(void);

#endif /* __TFTPD_H__ */

