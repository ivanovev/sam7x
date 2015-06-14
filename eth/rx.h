
#ifndef __RX_H__
#define __RX_H__

#include "uipopt.h"

void rx_init(void);
void rx_appcall(void);
void rx_efc(uint32_t sz, eventmask_t mask, char *md5);
uint8_t rx_status(void);
void rx_spi(uint8_t nspi, uint32_t sz);

#endif /* __RX_H__ */
