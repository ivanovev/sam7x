
#include "ch.h"
#include "uip.h"
#include "memb.h"
#include "events.h"

#include <string.h>

#include "tftpd.h"

#include "util.h"
#include "efc.h"
#include "pcl/picol.h"
#include "pcl/pcl_sam.h"

enum
{
    TFTP_RRQ    = 1,
    TFTP_WRQ    = 2,
    TFTP_DATA   = 3,
    TFTP_ACK    = 4,
    TFTP_ERR    = 5,
    TFTP_OACK   = 6,
    SEG_SZ      = 512,
};

enum
{
    TFTP_EUNDEF = 0,
    TFTP_ENOTFOUND,
    TFTP_EACCESS,
    TFTP_ENOSPACE,
    TFTP_EBADOP,
    TFTP_EBADID,
    TFTP_EEXISTS,
    TFTP_ENOUSER,
};

static struct errmsg {
    int16_t      e_code;
    const char  *e_msg;
} errmsgs[] = {
    {TFTP_EUNDEF,   "Undefined error"},
    {TFTP_ENOTFOUND,    "File not found"},
    {TFTP_EACCESS,  "Access violation"},
    {TFTP_ENOSPACE, "Disk full or allocation exceeded"},
    {TFTP_EBADOP,   "Illegal TFTP operation"},
    {TFTP_EBADID,   "Unknown transfer ID"},
    {TFTP_EEXISTS,  "File already exists"},
    {TFTP_ENOUSER,  "No such user"},
    {-1,            0}
};

#define UDPBUF ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct tftpd_state tfs;
extern Thread* maintp;

void tftpd_init(void)
{
    tfs.conn1 = uip_udp_new(0, 0);
    uip_udp_bind(tfs.conn1, HTONS(69));
    tfs.conn2 = uip_udp_new(0, 0);
    uip_udp_bind(tfs.conn2, HTONS(69));

    tfs.ackn = 0;
    tfs.mode = 0;
    //uip_listen(HTONS(69));
}
/*---------------------------------------------------------------------------*/
static void tftpd_ack(void)
{
    TFTP_ACK_PKT *ack = (TFTP_ACK_PKT*)uip_appdata;
    ack->opcode = HTONS(TFTP_ACK);
    ack->ackn = HTONS(tfs.ackn);
    tfs.ackn++;
    uip_send(uip_appdata, sizeof(TFTP_ACK_PKT));
}

static void tftpd_nak(int16_t e_code)
{
    TFTP_ERR_PKT *err = (TFTP_ERR_PKT*)uip_appdata;
    struct errmsg *e = 0;
    err->opcode = HTONS(TFTP_ERR);
    err->e_code = HTONS(e_code);
    uint16_t i, sz = 4;
    for(i = 0;; i++)
    {
        e = &errmsgs[i];
        if(e->e_code == e_code)
        {
            sz += snprintf(err->e_msg, SEG_SZ, "%s", e->e_msg);
            sz += 1;
            break;
        }
        if(e->e_code == -1)
            break;
    }
    //err->e_msg[sz] = 0;
    uip_send(uip_appdata, sz);
}

static void tftpd_data_in(uint8_t *data, uint16_t sz)
{
    if(tfs.mode == 0)
        return;
    TFTP_DATA_PKT *pkt = (TFTP_DATA_PKT*)data;
    sz -= 4;
    if(sz)
    {
        efc1_write_data(pkt->data, sz, tfs.start - EFC1_START, sz != SEG_SZ);
        tfs.start += sz;
    }
    tftpd_ack();
    if(sz != SEG_SZ)
    {
        tfs.ackn = 0;
    }
}

static void tftpd_wrq(uint8_t *data, uint16_t sz)
{
    tfs.crc = 0;
    tfs.mode = 0;
    tfs.ackn = 0;
    TFTP_PKT *pkt = (TFTP_PKT*)data; 
    char *name = (char*)pkt->data; 
    uint16_t len = strnlen(name, SEG_SZ);
    uint16_t i;
    char *mode = name + len + 1;
    print1("tftpd_wrq");
    if(strncmp(mode, "binary", SEG_SZ) && strncmp(mode, "octet", SEG_SZ))
    {   
        tftpd_nak(TFTP_EBADOP);
        return;
    }
    if(!strncmp(name, "script.pcl", 10))
    {
        tfs.trxdev = TRX_EFC;
    }
    else if((name[8] == '.') && !strncmp(&name[9], "bin", 3))
    {
        tfs.crc = strtoul(name, 0, 16);
        tfs.trxdev = TRX_EFC;
    }
    else
    {
        char tmp[64];
        snprintf("tftp_wrq_cb %s", sizeof(tmp), name);
        int ret = pcl_exec(tmp, 0);
        if(ret != PICOL_OK)
        {
            print1("file not found");
            return tftpd_nak(TFTP_ENOTFOUND);
        }
    }
    if(efc1_fsz())
    {
        efc1_erase();
        efc_ipaddr_write();
        efc_macaddr_write();
        efc_commit("eth", 0);
    }
    tfs.mode = TFTP_WRQ;
    tfs.start = EFC1_START;
    tfs.end = EFC1_END;
    tftpd_ack();
}

static void tftpd_data_out(uint8_t *data, uint16_t sz)
{
    print1("tftpd_data_out");
    if(tfs.mode == 0)
        return;
    TFTP_DATA_PKT *pkt = (TFTP_DATA_PKT*)data;
    pkt->opcode = HTONS(TFTP_DATA);
    pkt->ackn = HTONS(tfs.ackn);
    tfs.ackn++;
    uint16_t chunk_sz = SEG_SZ;
    if(chunk_sz >= (tfs.end - tfs.start))
    {
        chunk_sz = (tfs.end - tfs.start);
    }
    memcpy(pkt->data, (uint8_t*)tfs.start, chunk_sz);
    tfs.start += chunk_sz;
    uip_send(data, chunk_sz + 4);
    if(chunk_sz != SEG_SZ)
    {
        tfs.ackn = 0;
    }
}

static void tftpd_rrq(uint8_t *data, uint16_t sz)
{
    tfs.crc = 0;
    tfs.mode = 0;
    tfs.ackn = 1;
    TFTP_PKT *pkt = (TFTP_PKT*)data;
    char *name = (char*)pkt->data;
    uint16_t len = strnlen(name, SEG_SZ);
    char *mode = name + len + 1;
    print1("tftpd_rrq");
    print1(name);
    if(strncmp(mode, "binary", SEG_SZ) && strncmp(mode, "octet", SEG_SZ))
    {
        tftpd_nak(TFTP_EBADOP);
        tfs.ackn = 0;
        return;
    }
    if(!strncmp(name, "image", 5) && (name[6] == '.') && !strncmp(&name[7], "bin", 3))
    {
        print1("image");
        if(name[5] == '0')
        {
            tfs.start = EFC0_START;
            tfs.end = tfs.start + efc0_fsz();
        }
        else if(name[5] == '1')
        {
            tfs.start = EFC1_START;
            tfs.end = tfs.start + efc1_fsz();
        }
        else
        {
            tftpd_nak(TFTP_ENOTFOUND);
            tfs.ackn = 0;
            return;
        }
        tfs.trxdev = TRX_EFC;
        tfs.mode = TFTP_RRQ;
        tftpd_data_out(data, sz);
        return;
    }
    else if(!strncmp(name, "script.pcl", 10))
    {
        tfs.start = EFC1_START;
        tfs.end = tfs.start + efc1_fsz();
        tfs.trxdev = TRX_EFC;
        tfs.mode = TFTP_RRQ;
        tftpd_data_out(data, sz);
        return;
    }
    else
    {
        print1("file not found");
        return tftpd_nak(TFTP_ENOTFOUND);
    }
}

void tftpd_appcall(void)
{
    if(!uip_newdata())
        return;
    if(uip_udp_conn == tfs.conn1)
    {
        memcpy(tfs.conn1->ripaddr, UDPBUF->srcipaddr, sizeof(uip_ipaddr_t));
        tfs.conn1->rport = UDPBUF->srcport;
        memset(tfs.conn2->ripaddr, 0, sizeof(uip_ipaddr_t));
        tfs.conn2->rport = 0;
    }
    else if(uip_udp_conn == tfs.conn2)
    {
        memcpy(tfs.conn2->ripaddr, UDPBUF->srcipaddr, sizeof(uip_ipaddr_t));
        tfs.conn2->rport = UDPBUF->srcport;
        memset(tfs.conn1->ripaddr, 0, sizeof(uip_ipaddr_t));
        tfs.conn1->rport = 0;
    }
    else
        print1("conn?");
    TFTP_PKT *pkt = (TFTP_PKT*)uip_appdata;
    if(HTONS(pkt->opcode) == TFTP_WRQ)
        return tftpd_wrq(uip_appdata, uip_datalen());
    else if(HTONS(pkt->opcode) == TFTP_DATA)
        return tftpd_data_in(uip_appdata, uip_datalen());
    else if(HTONS(pkt->opcode) == TFTP_RRQ)
        return tftpd_rrq(uip_appdata, uip_datalen());
    else if(HTONS(pkt->opcode) == TFTP_ACK)
        return tftpd_data_out(uip_appdata, uip_datalen());
}

void tftpd_cb(void)
{
    if((tfs.mode != 0) && (tfs.ackn == 0))
    {
        //memset(tfs.conn->ripaddr, 0, sizeof(uip_ipaddr_t));
        //tfs.conn->rport = 0;
        if(tfs.crc)
        {
            if(tfs.crc == efc1_crc(efc1_fsz()))
            {
                chEvtSignal(maintp, EVT_FWUPG);
            }
        }
        else
        {
            if(efc1_fsz() != 0)
            {
                chEvtSignal(maintp, EVT_PCLUPD);
            }
        }
        tfs.crc = 0;
        tfs.mode = 0;
    }
}

/*---------------------------------------------------------------------------*/

