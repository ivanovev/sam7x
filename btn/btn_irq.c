
#include "ch.h"
#include "hal.h"
#include "events.h"
#include "btn_irq.h"

volatile uint32_t btn_isra, btn_isrb;
extern Thread *maintp;

CH_IRQ_HANDLER(PIOIrqHandler) {
    CH_IRQ_PROLOGUE();
    chSysLockFromIsr();
    btn_isra = AT91C_BASE_PIOA->PIO_ISR;
    btn_isrb = AT91C_BASE_PIOB->PIO_ISR;
	chEvtSignalI(maintp, EVT_BTN);
    AT91C_BASE_AIC->AIC_EOICR = 0;
    chSysUnlockFromIsr();
    CH_IRQ_EPILOGUE();
}

void btn_irq_init(uint8_t pio)
{
    if(pio == 0)
    {
        AIC_ConfigureIT(AT91C_ID_PIOA, AT91C_AIC_SRCTYPE_EXT_LOW_LEVEL | AT91SAM7_PIO_PRIORITY, PIOIrqHandler);
        AIC_EnableIT(AT91C_ID_PIOA);
    }
    else
    {
        AIC_ConfigureIT(AT91C_ID_PIOB, AT91C_AIC_SRCTYPE_EXT_LOW_LEVEL | AT91SAM7_PIO_PRIORITY, PIOIrqHandler);
        AIC_EnableIT(AT91C_ID_PIOB);
    }
}

void btn_irq_enable(uint8_t gpion)
{
    if(gpion < 31)
    {
        AT91C_BASE_PIOA->PIO_IFER = 1 << (gpion % 32);
        AT91C_BASE_PIOA->PIO_IER = 1 << (gpion % 32);
    }
    else
    {
        AT91C_BASE_PIOB->PIO_IFER = 1 << (gpion % 32);
        AT91C_BASE_PIOB->PIO_IER = 1 << (gpion % 32);
    }
}

