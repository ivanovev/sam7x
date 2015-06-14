
#include "ch.h"
#include "uip.h"
#include "rx.h"
#include "tx.h"
#include "efc.h"
#include "memb.h"
#include "events.h"

#include <string.h>

#include "util.h"
#include "spi1.h"

static struct trx_state rxs;
static char efcrxmd5sum[33];
eventmask_t efcrxevtmask;
extern Thread* maintp;

void rx_init(void)
{
    uip_listen(HTONS(8889));
}
void rx_efc(uint32_t sz, eventmask_t mask, char *md5)
{
    if(sz > EFC1_SZ)
        sz = EFC1_SZ;
    rxs.totalsz = sz;
    rxs.counter = 0;
    rxs.trxdev = TRX_EFC;
    efcrxevtmask = mask;
    strncpy(efcrxmd5sum, md5, 32);
    print2("rx_efc", sz);
}
void rx_spi(uint8_t nspi, uint32_t sz)
{
    rxs.trxdev = TRX_SPI;
    rxs.nspi = nspi;
    rxs.totalsz = sz;
    rxs.counter = 0;
    print2("rx_spi", nspi);
    print2("rx_sz", sz);
    spi1_select(nspi);
}
uint8_t rx_status(void)
{
    return (rxs.counter != rxs.totalsz);
}
/*---------------------------------------------------------------------------*/
static void recv_data(void)
{
    if((rxs.trxdev == TRX_EFC) && (rxs.counter < rxs.totalsz))
    {
        //print2("recv_data1", chTimeNow());
        int len = uip_datalen();
        if((rxs.counter + len) > EFC1_SZ)
            len = EFC1_SZ - rxs.counter;
        uint8_t efcrxend = (rxs.counter + len) >= rxs.totalsz;
        efc1_write_data(uip_appdata, len, rxs.counter, efcrxend);
        rxs.counter += len;
        if(efcrxend)
        {
            print1(efcrxmd5sum);
            char buf[33];
            efc1_md5(buf, -1);
            print1(buf);
            if(!strncmp(buf, efcrxmd5sum, 32))
            {
                print2("efcrxevtmask", efcrxevtmask);
                chEvtSignal(maintp, efcrxevtmask | EVT_CMDEND);
            }
        }
        //print2("recv_data2", chTimeNow());
        //print2("recv_data end", rxs.counter >= rxs.totalsz);
        return;
    }
    else if((rxs.trxdev == TRX_SPI) && (rxs.counter < rxs.totalsz))
    {
        u16_t len;
        len = uip_datalen();
        rxs.counter += len;
        //print2("newdata", len);
        spi1_write(rxs.nspi, (uint8_t*)uip_appdata, len);
    }
}
/*---------------------------------------------------------------------------*/
void rx_appcall(void)
{
    if(uip_newdata()) {
        recv_data();
    }
}
/*---------------------------------------------------------------------------*/

