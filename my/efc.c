
#pragma long_calls

#include "ch.h"
#include "hal.h"
#include "uip.h"
#include "crc.h"
#include "efc.h"
#include "util.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define EFC_PAGE_ETH_ADDR    0x3FF
#define EFC_PAGE_SPI	    0x3E0
#define EFC_PAGE_GPIO	    0x3A0
#define EFC_PAGE_SZ         0x100
#define EFC0_ADDR(p) (uint32_t*)(EFC0_START + (p << 8))
#define EFC1_ADDR(p) (uint32_t*)(EFC1_START + (p << 8))
#define EFCX_ADDR(start, p) (uint32_t*)(start + (p << 8))
#define ADDR_IS_EFC1(addr) ((EFC1_START <= addr) && (addr < EFC1_END))

char efc_databuf[EFC_PAGE_SZ];
int efc_counter = 0;

__attribute__ ((long_call, section (".ramtext"))) void efc1_command(int page, int command)
{
    uint32_t *ptr;
    ptr = (uint32_t*)0xFFFFFF74;
    *ptr = (0x5A << 24) | (page << 8) | AT91C_MC_NEBP | command;
    ptr = (uint32_t*)0xFFFFFF78;
    while(!(*ptr & 0x1));
}

void call_efc1_command_from_ram(int page, int command)
{
    chSysLock();
    efc1_command(page, command);
    chSysUnlock();
    chThdSleepMilliseconds(1);
}

void efc1_erase(void)
{
    call_efc1_command_from_ram(EFC_PAGE_ETH_ADDR, 8);
    chThdSleepMilliseconds(100);
}

uint32_t efc1_readw(uint16_t page, uint8_t offset)
{
    uint32_t *ptr = EFC1_ADDR((page));
    ptr += offset;
    return *ptr;
}

void efc1_writew(uint16_t page, uint8_t offset, uint32_t value)
{
    uint32_t *ptr = EFC1_ADDR(page);
    ptr += offset;
    if(ADDR_IS_EFC1(ptr))
        *ptr = value;
}

void efc_commit(char *intf, uint16_t n)
{
    uint16_t page = 0;
    if(!strncmp(intf, "gpio", 4) && (n < 64))
        page = EFC_PAGE_GPIO + n;
    else if(!strncmp(intf, "spi", 3) && (n < 32))
        page = EFC_PAGE_SPI + n;
    else if(!strncmp(intf, "eth", 3))
        page = EFC_PAGE_ETH_ADDR;
    else if(!strncmp(intf, "page", 4))
        page = n;
    print2("commit", page);
    if(page != 0)
        call_efc1_command_from_ram(page, 1);
}

uint8_t efc_gpio_read(uint8_t n)
{
    uint32_t *ptr = EFC1_ADDR((EFC_PAGE_GPIO + n));
    return *ptr;
}

void efc_gpio_write(uint8_t n, uint8_t v)
{
    if((v != 0) && (v != 1))
        return;
    efc1_writew(EFC_PAGE_GPIO + n, 0, v);
}

uint32_t efc_spi_read(uint8_t n, uint16_t offset)
{
    uint32_t *ptr = EFC1_ADDR((EFC_PAGE_SPI + n));
    ptr += offset;
    return *ptr;
}

int8_t efc_spi_readstr(char *buf, uint8_t n, uint16_t offset, uint8_t len)
{
    if((len == 0) || (len > 4))
        len = 4;
    char fmt[16];
    sprintf(fmt, "0x%%.%dX", len*2);
    unsigned int spi = (unsigned int)efc_spi_read(n, offset);
    spi = (spi & (0xFFFFFFFF >> 8*(4 - len)));
    sprintf(buf, fmt, spi);
    return 0;
}

void efc_spi_write(uint8_t n, uint16_t offset, uint32_t v)
{
    efc1_writew(EFC_PAGE_SPI + n, offset, v);
}

static int8_t efc_compare(uint8_t *data, uint32_t *efcdata, int len)
{
    int i;
    for(i = 0; i < len; i++)
    {
	if(*efcdata++ != data[i])
	    return 1;
    }
    return 0;
}

int8_t efc_ipaddr_write(void)
{
    uint32_t *ptr = EFC1_ADDR(EFC_PAGE_ETH_ADDR);
    *ptr = ip_addr.i32;
    return 0;
}

int8_t efc_ipaddr_read(void)
{
    uint32_t *ptr = EFC1_ADDR(EFC_PAGE_ETH_ADDR);
    if(*ptr == 0xFFFFFFFF)
    {
        ip_addr.i8[0] = 192;
        ip_addr.i8[1] = 168;
        ip_addr.i8[2] = 0;
        ip_addr.i8[3] = 1;
    }
    else
    {
        ip_addr.i32 = *ptr;
    }
    return 0;
}

int8_t efc_macaddr_write(void)
{
    uint32_t *ptr = EFC1_ADDR(EFC_PAGE_ETH_ADDR);
    ptr++;
    ptr[0] = mac_addr.i32[0];
    ptr[1] = mac_addr.i32[1];
    return 0;
}

void efc_macaddr_new(void)
{
    unsigned int s = chTimeNow();
    if(ip_addr.i32 != 0xFFFFFFFF)
        s |= ip_addr.i32;
    uint8_t i;
    for(i = 1; i < 6; i++)
    {
        mac_addr.i8[i] = (uint8_t)rand_r(&s);
    }
}

int8_t efc_macaddr_read(void)
{
    uint32_t *ptr = EFC1_ADDR(EFC_PAGE_ETH_ADDR);
    ptr++;
    if(*ptr == 0xFFFFFFFF)
    {
        efc_macaddr_new();
        return 0;
    }
    else
    {
        mac_addr.i32[0] = ptr[0];
        mac_addr.i32[1] = ptr[1];
    }
    return 0;
}

int8_t efc_get_erase_efc1(void)
{
    char *ptr0 = (char*)EFC0_ADDR(0);
    char *ptr1 = (char*)EFC1_ADDR(0);
    int8_t ret = !strncmp(ptr0, ptr1, 0x100);
    return ret;
}

void efc1_write_page(char *data, int len, int page)
{
    if(page > EFC_PAGE_ETH_ADDR)
        return;
    if(len)
    {
        uint32_t *ptr1 = EFC1_ADDR(page);
        uint32_t *ptr2 = (uint32_t*)data;
        uint8_t *ptr = (uint8_t*)ptr1;
        while(len % 4) data[len++] = 0xFF;
        uint8_t i;
        for(i = 0; i < len/4; i++)
            *ptr1++ = *ptr2++;
        //memcpy(ptr, data, len);
    }
    call_efc1_command_from_ram(page, 1);
}

void efc1_write_data(char *data, uint16_t datalen, uint32_t offset, uint8_t end)
{
    uint16_t page = offset / EFC_PAGE_SZ;
    if(offset == 0)
        efc_counter = 0;
    char *ptr = &(efc_databuf[efc_counter]);
    if((efc_counter + datalen) >= EFC_PAGE_SZ)
    {
        uint16_t datalen1 = EFC_PAGE_SZ - efc_counter;
        memcpy(ptr, data, datalen1);
        efc1_write_page(efc_databuf, EFC_PAGE_SZ, page);
        efc_counter = 0;
        datalen -= datalen1;
        offset += datalen1;
        if(datalen > 0)
            return efc1_write_data(&(data[datalen1]), datalen, offset, end);
    }
    if(datalen)
    {
        memcpy(&(efc_databuf[efc_counter]), data, datalen);
        efc_counter += datalen;
    }
    if(end)
    {
        print2("end counter", efc_counter);
        efc1_write_page(efc_databuf, efc_counter, page);
        uint32_t *ptr;
        while(++page < EFC_PAGE_ETH_ADDR)
        {
            uint32_t *ptr = (uint32_t*)EFC1_ADDR(page);
            if((ptr[0] != 0xFFFFFFFF) || (ptr[63] != 0xFFFFFFFF))
                call_efc1_command_from_ram(page, 1);
        }
    }
}

__attribute__ ((long_call, section (".ramtext"))) void efc0_command(int page, int command)
{
    uint32_t *ptr;
    ptr = (uint32_t*)0xFFFFFF64;
    *ptr = (0x5A << 24) | (page << 8) | AT91C_MC_NEBP | command;
    ptr = (uint32_t*)0xFFFFFF68;
    while(!(*ptr & 0x1));
}

__attribute__ ((long_call, section (".ramtext"))) void efc10_copy_efc1_to_efc0(void)
{
    uint32_t *ptr1, *ptr2;
    uint32_t i, j;
    ptr1 = (uint32_t*)0xFFFFF128;
    *ptr1 = 0xFFFFFFFF;
    ptr1 = (uint32_t*)0x140000;
    ptr2 = (uint32_t*)0x100000;
    for(i = 0; i < 1024; i++)
    {
	for(j = 0; j < 64; j++)
	    *ptr2++ = *ptr1++;
	efc0_command(i, 1);
    }
    ptr1 = (uint32_t*)0xFFFFFD00;
    *ptr1 = 0xA5000001;
    //ptr1 = (uint32_t*) 0xFFFFF630;
    //*ptr1 = 0x100000;
    while(1);
}

#if 1
void efc10_copy(void)
{
    efc10_copy_efc1_to_efc0();
}
#endif

#if 0
static uint32_t efc_get_length(uint8_t *ptr)
{
    //uint8_t *ptr = (uint8_t*)EFC1_ADDR(0);
    uint8_t *ptr0 = ptr;
    uint32_t *ptr1 = (uint32_t*)(ptr + 0x3DF00);
    while(ptr1[63] == 0xFFFFFFFF)
    {
        if(ptr1[0] != 0xFFFFFFFF)
            break;
        if(ptr1 == (uint32_t*)ptr0)
            return 0;
        ptr1 -= 64;
    }
    ptr1 += 64;
    ptr = (uint8_t*)ptr1;
    while(*--ptr == 0xFF);
    return (ptr - ptr0) + 1;
}
#else
static uint32_t efc_get_length(uint32_t start)
{
    uint16_t page = EFC_PAGE_GPIO - 1;
    uint32_t *ptr = EFCX_ADDR(start, page);
    if((ptr[0] != 0xFFFFFFFF) || (ptr[63] != 0xFFFFFFFF))
        page = EFC_PAGE_ETH_ADDR - 1;
    for(; page > 0; page--)
    {
        ptr = EFCX_ADDR(start, page);
        if((ptr[0] != 0xFFFFFFFF) || (ptr[63] != 0xFFFFFFFF))
            break;
    }
    uint8_t *ptr1 = (uint8_t*)EFCX_ADDR(start, page) + EFC_PAGE_SZ - 1;
    //print2("ptr1", ptr1);
    while(--ptr1 > start)
        if(*(ptr1-1) != 0xFF)
            break;
    return (ptr1 - start);
}
#endif

uint32_t efc0_crc(int32_t len)
{
    if(len == -1)
        len = efc0_fsz();
    return crc32(0, (void*)EFC0_START, len);
}

uint32_t efc1_crc(int32_t len)
{
    if(len == -1)
        len = efc1_fsz();
    return crc32(0, (void*)EFC1_START, len);
}

uint32_t efc0_fsz(void)
{
    return efc_get_length(EFC0_START);
}

uint32_t efc1_fsz(void)
{
    return efc_get_length(EFC1_START);
}

