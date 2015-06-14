
#include <stdlib.h>
#include "ch.h"
#include "hal.h"
#include "pal.h"
//#include "httpd.h"
#include "efc.h"
#include "gpio.h"
#include "util.h"
#include <string.h>

#define IOPORT(gpio) ((gpio < 31) ? IOPORT1 : IOPORT2)

#if 0
void gpio_configure_pins(void)
{
    int line_count = file_get_line_count("/gpio.list");
    int i, gpio;
    uint32_t pioa = 0, piob = 0;
    char str[MAX_STR_LEN];
    for(i = 0; i < line_count; i++)
    {
	int ret = file_get_str_n("/gpio.list", i, str);
	if(!ret)
	    break;
	gpio = atoi(str);
	if(gpio < 32)
	    pioa |= 1 << gpio;
	else
	    piob |= 1 << gpio;
    }
    AT91C_BASE_PIOA->PIO_PER = pioa;
    AT91C_BASE_PIOA->PIO_OER = pioa;
    AT91C_BASE_PIOA->PIO_CODR = pioa;
    AT91C_BASE_PIOB->PIO_PER = piob;
    AT91C_BASE_PIOB->PIO_OER = piob;
    AT91C_BASE_PIOB->PIO_CODR = piob;
}
#endif

#if 0
void gpio_restore_saved(void)
{
    uint8_t i, v;
    for(i = 0; i < 64; i++)
    {
	v = efc_gpio_read(i);
	if((v == 0) || (v == 1))
	{
	    gpio_write(i, v);
	}
    }
}
#endif

void gpio_write(uint8_t n, uint8_t v)
{
    palSetPadMode(IOPORT(n), n % 32, PAL_MODE_OUTPUT_PUSHPULL);
    if(v == 1)
    {
        palSetPad(IOPORT(n), n % 32);
    }
    else
    {
        palClearPad(IOPORT(n), n % 32);
    }
}

uint8_t gpio_read(uint8_t n)
{
    return palReadPad(IOPORT(n), n % 32);
}

uint8_t gpio_odsr(uint8_t n)
{
    uint32_t *ptr = 0, val;
    if(n < 32)
        ptr = (uint32_t*)0xFFFFF438;
    else
        ptr = (uint32_t*)0xFFFFF638;
    n = n % 32;
    return (*ptr & (1 << n)) ? 1 : 0;
}

void gpio_input_all(void)
{
    int i;
    for(i = 0; i < 64; i++)
	palSetPadMode(IOPORT(i), i % 32, PAL_MODE_INPUT);
}

void gpio_led_on(void)
{
    palSetPad(IOPORT2, PIOB_LCD_BL);
}
void gpio_led_off(void)
{
    palClearPad(IOPORT2, PIOB_LCD_BL);
}

void gpio_led_blink(uint8_t n)
{
    uint8_t i;
    for(i = 0; i < n; i++)
    {
	gpio_led_on();
	chThdSleepMilliseconds(200);
	gpio_led_off();
	if(n == 1) break;
	chThdSleepMilliseconds(200);
    }
}

uint32_t gpio_get_reg(char *name, uint8_t pio)
{
    AT91PS_PIO piox = NULL;
    if(pio == 0)
	piox = AT91C_BASE_PIOA;
    if(pio == 1)
	piox = AT91C_BASE_PIOB;
    uint32_t *reg = NULL;
    if(!strncmp(name, "psr", 3))
    {
	reg = (uint32_t*)&(piox->PIO_PSR);
    }
    if(!strncmp(name, "absr", 4))
    {
	reg = (uint32_t*)&(piox->PIO_ABSR);
    }
    if(!strncmp(name, "pusr", 4))
    {
	reg = (uint32_t*)&(piox->PIO_PPUSR);
    }
    if(!strncmp(name, "osr", 3))
    {
	reg = (uint32_t*)&(piox->PIO_OSR);
    }
    if(reg)
	return *reg;
    return 0;
}

void gpio_set_reg(char *name, char *val, uint8_t pio)
{
    AT91PS_PIO piox = NULL;
    if(pio == 0)
        piox = AT91C_BASE_PIOA;
    if(pio == 1)
        piox = AT91C_BASE_PIOB;
    uint32_t *reg = NULL;
    if(!strncmp(name, "per", 3))
    {
        reg = (uint32_t*)&(piox->PIO_PER);
    }
    if(!strncmp(name, "pdr", 3))
    {
        reg = (uint32_t*)&(piox->PIO_PDR);
    }
    if(!strncmp(name, "asr", 3))
    {
        reg = (uint32_t*)&(piox->PIO_ASR);
    }
    if(!strncmp(name, "bsr", 3))
    {
        reg = (uint32_t*)&(piox->PIO_BSR);
    }
    if(!strncmp(name, "pudr", 4))
    {
        reg = (uint32_t*)&(piox->PIO_PPUDR);
    }
    if(!strncmp(name, "puer", 4))
    {
        reg = (uint32_t*)&(piox->PIO_PPUER);
    }
    if(!strncmp(name, "oer", 3))
    {
        reg = (uint32_t*)&(piox->PIO_OER);
    }
    if(!strncmp(name, "odr", 3))
    {
        reg = (uint32_t*)&(piox->PIO_ODR);
    }
    if(reg)
    {
        *reg = str2int(val);
    }
}

