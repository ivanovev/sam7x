
#ifndef __UTIL_H_
#define __UTIL_H_

unsigned int	rnd_seed(void);

void    print_info(void);
void    print1(const char *str);
void    print2(const char *str, uint32_t n);
void    print3(const char *str, uint8_t count);
void    print4(const char *str);
void    print5(const char *str);

int		version_str(char *buf, int len);
int		version_date_str(char *buf, int len);
int		ipaddr_check(uint8_t addr[4]);
int		ipaddr_str(char *buf, int len);
int		macaddr_str(char *buf, int len);
int		uptime_str(char *buf, int len, int8_t ms);
int		telnetd_str(char *buf, int len);

uint32_t	str2int(const char *str);
int		str2bytes(const char *in, uint8_t *out, int maxlen);
int		double2str(char *buf, int len, double f, unsigned int p);
float		interpolate(float x1, float y1, float x2, float y2, float x);
void		sdw(uint8_t sd, uint8_t *str, int len);
uint8_t		sdr(uint8_t sd, uint8_t *buf, int len, uint8_t end);
uint8_t		sd(uint8_t sd, uint8_t *buf, int len, uint8_t end);
void		sdloop(uint8_t sd, uint8_t *str, int len);
#if VFD
int		utf8_to_cp866(char *in, char *out, uint32_t maxlen);
#endif

#endif
