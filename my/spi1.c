
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "spi_lld.h"

#include "spi1.h"
#include "efc.h"
#include "gpio.h"
#include "util.h"

#define IOPORT(n) ((n <= 15) ? IOPORT1 : IOPORT2)
#define SPIDRV(n) ((n <= 15) ? &SPID1 : &SPID2)
#define SPIVALID(n) ((0 <= n) && (n <= 19))

#define SPI1_SCBR 0x80
//#define SPI1_SCBR 0x07
#define SPI2_SCBR 0x80

static SPIConfig spicfg1 = {
    NULL,
    IOPORT1,
    AT91C_PA12_SPI0_NPCS0,
    (SPI1_SCBR << 8) | AT91C_SPI_NCPHA | AT91C_SPI_CPOL
};

static SPIConfig spicfg2 = {
    NULL,
    IOPORT2,
    AT91C_PA21_SPI1_NPCS0,
    (SPI2_SCBR << 8) | AT91C_SPI_NCPHA | AT91C_SPI_CPOL
};

#define SPICFG(n) ((n <= 15) ? spicfg1 : spicfg2)

void spi1_init(void)
{
    //palSetPadMode(IOPORT1, 16, PAL_MODE_INPUT_PULLUP);
    //palSetPadMode(IOPORT1, 24, PAL_MODE_INPUT_PULLUP);
#if 1
    //spi_lld_init();
    spiStart(&SPID1, &spicfg1);
    spiStart(&SPID2, &spicfg2);
    uint8_t i;
    for(i = 0; i < 4; i++)
    {
        AT91C_BASE_SPI0->SPI_CSR[i] = spicfg2.csr;
        AT91C_BASE_SPI1->SPI_CSR[i] = spicfg1.csr;
    }
    AT91C_BASE_PIOA->PIO_PDR = AT91C_PA12_SPI0_NPCS0 | AT91C_PA13_SPI0_NPCS1 | AT91C_PA14_SPI0_NPCS2 | AT91C_PA15_SPI0_NPCS3 | AT91C_PA21_SPI1_NPCS0 | AT91C_PA25_SPI1_NPCS1 | AT91C_PA26_SPI1_NPCS2;
    AT91C_BASE_PIOA->PIO_ASR = AT91C_PA12_SPI0_NPCS0 | AT91C_PA13_SPI0_NPCS1 | AT91C_PA14_SPI0_NPCS2 | AT91C_PA15_SPI0_NPCS3;
    AT91C_BASE_PIOA->PIO_BSR = AT91C_PA21_SPI1_NPCS0 | AT91C_PA25_SPI1_NPCS1 | AT91C_PA26_SPI1_NPCS2;
#endif
}

uint32_t* spix_find_csr(uint8_t spi)
{
    uint32_t *ptr = 0;
    //ptr0 = (uint32_t*)0xFFFE0030;
    //ptr1 = (uint32_t*)0xFFFE4030;
    if(spi <= 15)
        ptr = (uint32_t*)&AT91C_BASE_SPI0->SPI_CSR[spi/4];
    else if((16 <= spi) && (spi <= 19))
        ptr = (uint32_t*)&AT91C_BASE_SPI1->SPI_CSR[spi - 16];
    return ptr;
}

void spi1_csr_bits(char *buf, uint8_t nspi, uint8_t start, uint8_t stop, int32_t val)
{
    if(!SPIVALID(nspi))
        return;
    uint32_t mask = ((1 << (stop - start + 1)) - 1) << start;
    //print2("csr mask", mask);
    uint32_t *ptr = spix_find_csr(nspi);
    uint32_t ret = 0;
    if(val == -1)
        ret = (*ptr & mask) >> start;
    else
    {
        ret = *ptr & ~mask;
        ret = ret | ((val << start) & mask);
        *ptr = ret;
    }
    if(buf)
        sprintf(buf, "0x%.8X", ret);
}

static int8_t spi1_csr_upd(uint8_t spi, int8_t bit, uint32_t mask)
{
    uint32_t val;
    uint32_t *ptr = spix_find_csr(spi);
    if(!ptr)
        return bit;
	val = *ptr;
    if((bit == 0) || (bit == 1))
    {
        if(bit)
            val = val | mask;
        else
            val = val & ~mask;
        *ptr = val;
    }
    else
        bit = (val & mask) ? 1 : 0;
    return bit;
}

int8_t spi1_ncpha(uint8_t spi, int8_t ncpha)
{
    return spi1_csr_upd(spi, ncpha, AT91C_SPI_NCPHA);
}

int8_t spi1_cpol(uint8_t spi, int8_t cpol)
{
    return spi1_csr_upd(spi, cpol, AT91C_SPI_CPOL);
}

static AT91PS_SPI spi1_choose(uint8_t nspi)
{
    AT91PS_SPI pspi = 0;
    if(nspi <= 15)
        pspi = AT91C_BASE_SPI0;
    else if((16 <= nspi) && (nspi <= 19))
        pspi = AT91C_BASE_SPI1;
}

int32_t spi1_get_reg(uint8_t nspi, char *reg)
{
    uint32_t *ptr;
    if(!SPIVALID(nspi))
        return;
    if(!strncmp(reg, "csr", 3))
        ptr = spix_find_csr(nspi);
    else
    {
        AT91PS_SPI pspi = spi1_choose(nspi);
#if 0
        if(nspi <= 15)
            pspi = AT91C_BASE_SPI0;
        else if((16 <= nspi) && (nspi <= 19))
            pspi = AT91C_BASE_SPI1;
#endif
        if(!strncmp(reg, "cr", 2))
            ptr = &pspi->SPI_CR;
        if(!strncmp(reg, "mr", 2))
            ptr = &pspi->SPI_MR;
    }
    //print2(reg, nspi);
    //print2("register addr", ptr);
    if(ptr)
        return *ptr;
    return 0;
}

#define SPI0_NPCS0 12
#define SPI0_NPCS1 13
#define SPI0_NPCS2 14
#define SPI0_NPCS3 15

#define SPI1_NPCS0 21
#define SPI1_NPCS1 25
#define SPI1_NPCS2 26
#define SPI1_NPCS3 29

void spi1_select(uint32_t nspi)
{
    uint32_t *mr, mode = AT91C_SPI_MSTR | AT91C_SPI_MODFDIS;
    if(!SPIVALID(nspi))
        return;
    if(nspi <= 15)
    {
        mode = mode | AT91C_SPI_PS_FIXED | AT91C_SPI_PCSDEC;
        mr = (uint32_t*)&AT91C_BASE_SPI0->SPI_MR;
    }
    else if((16 <= nspi) && (nspi <= 19))
    {
        mode = mode | AT91C_SPI_PS_FIXED;
        mr = (uint32_t*)&AT91C_BASE_SPI1->SPI_MR;
        nspi -= 16;
        nspi = ~(1 << nspi);
    }
    mode = ((nspi << 16) & AT91C_SPI_PCS) | mode;
    //print2("spi mr", mode);
    *mr = mode;
    //print2("spi mr", *mr);
	//gpio_write(50, 0);
}

void spi1_write(int n, uint8_t *tx, int len)
{
    //spi1_select(n);
    int i, l1, L = 512;
    AT91PS_SPI pspi = spi1_choose(n);
    int bytes = ((pspi->SPI_MR & AT91C_SPI_BITS) == AT91C_SPI_BITS_8) ? 1 : 2;
    if(len % 2)
        print1("spi_write: len is odd!!!"); 
    for(i = 0;;)
    {
        if((len - i) > L)
            l1 = L;
        else
            l1 = len - i;
        spiSend(SPIDRV(n), l1/bytes, &tx[i]);
        while((pspi->SPI_SR & AT91C_SPI_ENDTX) == 0)
            chThdSleepMilliseconds(1);
        i += l1;
        if(i == len)
            break;
    }
}

void spi1_read(int n, uint8_t *rx, int len)
{
    //spi1_select(n);
    AT91PS_SPI pspi = spi1_choose(n);
    int bytes = ((pspi->SPI_MR & AT91C_SPI_BITS) == AT91C_SPI_BITS_8) ? 1 : 2;
    if(len % 2)
        print1("spi_write: len is odd!!!"); 
    spiReceive(SPIDRV(n), len/bytes, rx);
}

#if 0
void spi1_write1(int n, uint32_t w, int len)
{
    uint8_t tx[4];
    tx[3] = w & 0xFF;
    tx[2] = (w >> 8) & 0xFF;
    tx[1] = (w >> 16) & 0xFF;
    tx[0] = (w >> 24) & 0xFF;
    spi1_write(n, tx, len);
}
#endif

#define MAX_SPI_LEN 4

struct _spi_msg
{
    int			n;
    uint8_t		tx[MAX_SPI_LEN];
    int			len;
    volatile uint8_t	loop;
} spi_msg;


static WORKING_AREA(waLoopThread, 128);
static msg_t LoopThread(void *p)
{
    (void)p;
    chRegSetThreadName("spi_loop");
    //print2("spi loop start", spi_msg.n);
    while(!chThdShouldTerminate())
    {
        spiSend(SPIDRV(spi_msg.n), spi_msg.len, spi_msg.tx);
        //spi1_write(spi_msg.n, spi_msg.tx, spi_msg.len);
    }
    //print1("spi loop end");
    return 0;
}

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

int spi1_io(int nspi, char *in, char *out, int loop)
{
    static Thread *spi1tp = NULL;
    if(spi1tp)
    {
        chThdTerminate(spi1tp);
        chThdWait(spi1tp);
        spi1tp = NULL;
    }
#if VERBOSE_IO
    if(!loop)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "spi%d %s", nspi, in);
        print1(buf);
    }
#endif
    uint8_t *tx = spi_msg.tx, rx[MAX_SPI_LEN];
    int len;
    len = str2bytes(in, tx, MAX_SPI_LEN);
    //print2("spi len", len);
    spi1_csr_bits(0, nspi, 4, 7, 0);
    spi1_select(nspi);
    if(loop)
    {
        spi_msg.n = nspi;
        spi_msg.len = len;
        spi1tp = chThdCreateStatic(waLoopThread, sizeof(waLoopThread), LOWPRIO, LoopThread, NULL);
        return 0;
    }
    //print1("spi start");
    spiExchange(SPIDRV(nspi), len, tx, rx);    /* Polled method.       */
    //print1("spi end");
    int i;
    for(i = 0; i < len; i++)
    {
        sprintf(&(out[2*i]), "%.2X", rx[i]);
    }
    rx[len] = 0;
    return len;
}

int spi1_get_temp(float *temp, int8_t nspi)
{
    if(!SPIVALID(nspi))
        return 0;
    char *in = "0000";
    char out[16];
    spi1_cpol(nspi, 1);
    spi1_ncpha(nspi, 0);
    spi1_select(nspi);
    spi1_io(nspi, in, out, 0);
    if(!strncmp(in, out, 4))
        return -1;
    out[4] = 0;
    uint16_t t = (uint16_t)strtoul(out, 0, 16);
    t = (t >> 5);
    if(t & (1 << 9))
        *temp = interpolate(0x200, -128., 0x3FF, -0.25, t);
    else if(!(t & 0x3))
        *temp = interpolate(0x0, 0., 0xFC, 127., t);
    else
        *temp = 0;
    return 0;
}

