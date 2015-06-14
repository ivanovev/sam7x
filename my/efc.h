
#ifndef __EFC_H_
#define __EFC_H_

#define EFC0_START 0x100000
#define EFC1_START 0x140000
#define EFC1_END 0x180000
#define EFC1_SZ EFC1_END - EFC1_START

union IP_Addr
{
    uint32_t i32;
    uint8_t i8[4];
} ip_addr;

union MAC_Addr
{
    uint32_t i32[2];
    uint8_t i8[6];
    uint8_t addr[6];
} mac_addr;

void	    efc_init(void);

uint32_t    efc1_readw(uint16_t page, uint8_t offset);
void        efc1_writew(uint16_t page, uint8_t offset, uint32_t value);
void        efc_commit(char *intf, uint16_t n);

uint8_t	    efc_gpio_read(uint8_t n);
void	    efc_gpio_write(uint8_t n, uint8_t v);
uint32_t    efc_spi_read(uint8_t n, uint16_t offset);
int8_t	    efc_spi_readstr(char *buf, uint8_t n, uint16_t offset, uint8_t len);
void	    efc_spi_write(uint8_t n, uint16_t offset, uint32_t v);

int8_t	    efc_ipaddr_read(void);
int8_t	    efc_ipaddr_write(void);
void	    efc_macaddr_new(void);
int8_t	    efc_macaddr_read(void);
int8_t	    efc_macaddr_write(void);

void	    efc1_erase(void);
//void	    efc1_erase_page(uint32_t page);

int8_t	    efc_get_erase_efc1(void);

//void	    efc1_write(char *data, int len, int offset);
void	    efc1_write_page(char *data, int len, int page);
void        efc1_write_data(char *data, uint16_t datalen, uint32_t offset, uint8_t end);
//void	    efc_copy(void);
void	    efc10_copy(void);

uint32_t	efc0_crc(int32_t len);
uint32_t	efc1_crc(int32_t len);
uint32_t    efc0_fsz(void);
uint32_t    efc1_fsz(void);

#define	    efc_spi_read_len(n) efc_spi_read(n, 63)

#endif
