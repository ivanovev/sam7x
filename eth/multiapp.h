
#ifndef __MULTIAPP_H__
#define __MULTIAPP_H__

#include "telnetd.h"
#include "tftpd.h"
#include "my/util.h"

extern void tcp_appcall(void);
extern void udp_appcall(void);
extern void uip_log(char *s);

typedef union {
struct telnetd_state uip_tcp_telnet;
} uip_tcp_appstate_t;

typedef union {
struct tftpd_state uip_udp_tftp;
} uip_udp_appstate_t;

#define UIP_APPCALL tcp_appcall

#define UIP_UDP_APPCALL udp_appcall

#endif /*__MULTIAPP_H__*/

