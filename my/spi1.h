
#ifndef __SPI1_H_
#define __SPI1_H_

void	spi1_init(void);
void    spi1_csr_bits(char *buf, uint8_t nspi, uint8_t start, uint8_t stop, int32_t val);
int32_t spi1_get_reg(uint8_t spi, char *reg);
void	spi1_select(uint32_t nspi);
void	spi1_write(int n, uint8_t *tx, int len);
void	spi1_read(int n, uint8_t *rx, int len);
int     spi1_io(int n, char *in, char *out, int loop);
int     spi1_get_temp(float *temp, int8_t n);

#endif
