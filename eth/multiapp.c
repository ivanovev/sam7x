
#include "uip.h"
#include "telnetd.h"
#include "tftpd.h"
#include "multiapp.h"

void tcp_appcall(void)
{
    switch (uip_conn->lport)
    {
	case HTONS(23):
	    telnetd_appcall();
	    break;

	default: /* Should never happen. */
	    uip_abort();
	    break;
    }
}

void udp_appcall(void)
{
    switch (uip_udp_conn->lport)
    {
	case HTONS(69):
	    tftpd_appcall();
	    break;

	default: /* Should never happen. */
	    uip_abort();
	    break;
    }
}

void uip_log(char *s)
{
    print1(s);
}


