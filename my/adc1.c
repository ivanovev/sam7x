
#include "ch.h"
#include "hal.h"
#include "adc1.h"

void adc1_init(void)
{
    // enable channels
    AT91C_BASE_ADC->ADC_CHER = 0xFF;
    // reset adc
    AT91C_BASE_ADC->ADC_CR = 0x1;
    AT91C_BASE_ADC->ADC_CR = 0x0;
    // startup & hold time -> max
    AT91C_BASE_ADC->ADC_MR = 0x0F1F0F00;
    // enable channels
    AT91C_BASE_ADC->ADC_CHER = 0xFF;
    //disable interrupts
    AT91C_BASE_ADC->ADC_IDR = 0x000FFFFF;
}

uint16_t adc1_read(int chan)
{
    if((chan < 0) || (7 < chan))
	return 0;
    
    //print1("start adc");
    adc1_init();
    AT91C_BASE_ADC->ADC_CHER = (1 << chan);
    AT91C_BASE_ADC->ADC_CR = 0x2;
    while(!(AT91C_BASE_ADC->ADC_SR & (1 << chan)));
    uint32_t *ptr = (uint32_t*)&(AT91C_BASE_ADC->ADC_CDR0);
    ptr += chan;
    uint16_t res = *ptr;
    //print1("end adc");
    return res;
}

