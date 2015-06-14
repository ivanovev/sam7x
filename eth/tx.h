
#ifndef __TX_H__
#define __TX_H__

#include "uipopt.h"

void tx_init(void);
void tx_appcall(void);
void tx_efc(uint32_t sz);

#ifndef TRX_LEN
#define TRX_LEN 512
#endif

#define TRX_NONE 0
#define TRX_EFC 1
#define TRX_SPI 2

struct trx_state {
    uint8_t trxdev;
    uint8_t nspi;
    uint32_t totalsz;
    uint32_t counter;
};

#endif /* __TX_H__ */
