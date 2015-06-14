
#ifndef __UART1_H_
#define __UART1_H_

void	uart1_init(void);
int     uart1_io(int8_t uartn, char *buf, int8_t len, int32_t timeout, uint8_t loop, char endch);

#endif
