
#include "ch.h"
#include "hal.h"
#include "vfd.h"
#include "vfd_irq.h"

uint32_t vfd_isr;
extern Thread *maintp;

CH_IRQ_HANDLER(PIOBIrqHandler) {
    CH_IRQ_PROLOGUE();
    chSysLockFromIsr();
    vfd_isr = AT91C_BASE_PIOB->PIO_ISR;
    if(vfd_isr & (1 << 21))
	chEvtSignalFlagsI(maintp, VFD_LEFT);
    else if(vfd_isr & (1 << 23))
	chEvtSignalFlagsI(maintp, VFD_RIGHT);
    else if(vfd_isr & (1 << 24))
	chEvtSignalFlagsI(maintp, VFD_DOWN);
    else if(vfd_isr & (1 << 25))
	chEvtSignalFlagsI(maintp, VFD_UP);
    AT91C_BASE_AIC->AIC_EOICR = 0;
    chSysUnlockFromIsr();
    CH_IRQ_EPILOGUE();
}

void vfd_irq_init(void)
{
    AIC_ConfigureIT(AT91C_ID_PIOB, AT91C_AIC_SRCTYPE_EXT_LOW_LEVEL | AT91SAM7_PIO_PRIORITY, PIOBIrqHandler);
    AIC_EnableIT(AT91C_ID_PIOB);
    AT91C_BASE_PIOB->PIO_IFER = (1 << 21) | (1 << 23) | (1 << 24) | (1 << 25);
    AT91C_BASE_PIOB->PIO_IER = (1 << 21) | (1 << 23) | (1 << 24) | (1 << 25);
}

