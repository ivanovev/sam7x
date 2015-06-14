
#include "uip.h"
#include "tx.h"
#include "efc.h"
#include "memb.h"

#include <string.h>

#include "util.h"
#include "spi1.h"
#include "gpio.h"

static struct trx_state txs;

void tx_init(void)
{
    uip_listen(HTONS(8888));
    txs.trxdev = TRX_NONE;
}

void tx_efc(uint32_t sz)
{
    if(sz > EFC1_SZ)
        sz = EFC1_SZ;
    txs.totalsz = sz;
    txs.counter = 0;
    txs.trxdev = TRX_EFC;
    //print2("efc tx", sz);
}
void tx_spi(uint8_t nspi, uint32_t sz)
{
    txs.trxdev = TRX_SPI;
    txs.nspi = nspi;
    txs.totalsz = sz;
    txs.counter = 0;
    print2("tx_spi", nspi);
    print2("tx_spi", sz);
    spi1_select(nspi);
}
/*---------------------------------------------------------------------------*/
static void send_data(void)
{
    if((txs.trxdev == TRX_EFC) && (txs.counter < txs.totalsz))
    {
        char *data = (char*)(EFC1_START + txs.counter);
        int len = TRX_LEN;
        if((txs.totalsz - txs.counter) < TRX_LEN)
            len = txs.totalsz - txs.counter;
        uip_send(data, len);
        txs.counter += len;
        return;
    }
    if((txs.trxdev == TRX_SPI) && (txs.counter < txs.totalsz))
    {
        size_t len = TRX_LEN;
        if(len <= uip_mss())
        {
            spi1_read(txs.nspi, uip_appdata, len);
            uip_send(uip_appdata, len);
            txs.counter += len;
        }
    }
}
/*---------------------------------------------------------------------------*/
#if 0
static void
closed(void)
{
}
#endif
/*---------------------------------------------------------------------------*/
#if 0
static void
newdata(void)
{
    u16_t len;
    u8_t c;
    char *dataptr;
    len = uip_datalen();
    dataptr = (char *)uip_appdata;
    spi1_write(16, dataptr, len);
    //print1(dataptr);
}
#endif
/*---------------------------------------------------------------------------*/
void
tx_appcall(void)
{
#if 1
    if(uip_connected() || uip_acked())
    {
	//if(gpio_read(11))
	    send_data();
    }
#endif
}
/*---------------------------------------------------------------------------*/
