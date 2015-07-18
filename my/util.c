
#include "ch.h"
#include "hal.h"
#include "uip.h"
#include "util.h"
#include "adc1.h"
#include "efc.h"
#if VFD
#include "cp866/cp866.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t rnd_seed(void)
{
    uint32_t s = 0;
    uint8_t i;
    for(i = 0; i < 8; i++)
    {
	s += adc1_read(i);
    }
    return s;
}

void print_info(void)
{
    char buf[24];
    ipaddr_str(buf, sizeof(buf));
    print1(buf);
    macaddr_str(buf, sizeof(buf));
    print1(buf);
    version_str(buf, sizeof(buf));
    print1(buf);
    print2("mem:", chCoreStatus());
}

void print1(const char *str)
{
    int len = strlen(str);
    if(len > 256) len = 256;
    if(len)
    {
        sdWrite(&SDDBG, (uint8_t *)str, len);
        sdWrite(&SDDBG, (uint8_t *)"\n\r", 2);
    }
}

void print2(const char *str, uint32_t n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s = %.8X\n\r", str, n);
    print1(buf);
}

void print3(const char *str, uint8_t count)
{
    int len, sz = strlen(str);
    char *ptr = str;
    char *ptr1 = str;
    char buf[64];
    while(1)
    {
        ptr1 = memchr(ptr, 0x0A, sz);
        if(!ptr1)
            break;
        if(!count--)
            break;
        len = ptr1 - ptr + 1;
        snprintf(buf, (len > sizeof(buf)) ? sizeof(buf) : len, "%s", ptr);
        print1(buf);
        while(ptr1[0] == 0x0A)
        {
            ptr1++;
            len++;
        }
        ptr = ptr1;
        sz = sz - len;
        if(sz <= 0)
            break;
    }
}

void print4(const char *str)
{
    print3(str, 255);
}

void print5(const char *str)
{
    print3(str, 1);
}

int version_str(char *buf, int len)
{
    return snprintf(buf, len, "%s %s", __DATE__, __TIME__);
}

int version_date_str(char *buf, int len)
{
    return snprintf(buf, len, "%s", __DATE__);
}

int ipaddr_check(uint8_t addr[4])
{
    uint8_t i;
    for(i = 0; i < 4; i++)
    {
        if(addr[i] == 255)
            return -1;
    }
    if(!addr[0] || !addr[3])
        return -1;
    return 0;
}

int ipaddr_str(char *buf, int len)
{
    efc_ipaddr_read();
    return snprintf(buf, len, "%d.%d.%d.%d", ip_addr.i8[0], ip_addr.i8[1], ip_addr.i8[2], ip_addr.i8[3]);
}

int macaddr_str(char *buf, int len)
{
    efc_macaddr_read();
    uint8_t *ma = mac_addr.i8;
    return snprintf(buf, len, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", ma[0], ma[1], ma[2], ma[3], ma[4], ma[5]);
}

int uptime_str(char *buf, int len, int8_t ms)
{
    if(len < 20)
	return 0;
    unsigned int t = chTimeNow();
    int ret;
    if(ms)
	ret = snprintf(buf, len, "%u", t);
    else
    {
	t /= CH_FREQUENCY;
	int ss = t % 60;
	t /= 60;
	int mm = t % 60;
	t /= 60;
	int hh = t % 24;
	t /= 24;
	int dd = t;
	ret = snprintf(buf, len, "%.2dd %.2dh %.2dm %.2ds", dd, hh, mm, ss);
    }
    return ret;
}

extern uip_ipaddr_t telnetd_ripaddr;
extern unsigned int telnetd_rport;
int telnetd_str(char *buf, int len)
{
    uint8_t a[4];
    a[0] = telnetd_ripaddr[0] & 0xFF;
    a[1] = (telnetd_ripaddr[0] >> 8) & 0xFF;
    a[2] = telnetd_ripaddr[1] & 0xFF;
    a[3] = (telnetd_ripaddr[1] >> 8) & 0xFF;
    if(telnetd_rport)
	return snprintf(buf, len, "%d.%d.%d.%d:%d", a[0], a[1], a[2], a[3], telnetd_rport);
    else
	return snprintf(buf, len, "idle");
}

uint32_t str2int(const char *str)
{
    const char *ptr = str + 2;
    if(!strncmp(str, "0x", 2))
	return strtoul(ptr, 0, 16);
    if(!strncmp(str, "0b", 2))
	return strtoul(ptr, 0, 2);
    if(!strncmp(str, "0o", 2))
	return strtoul(ptr, 0, 8);
    return strtoul(str, 0, 10);
}

int str2bytes(const char *in, uint8_t *out, int maxlen)
{
    int len;
    char ch[2];
    const char *tmp;
    if(!strncmp(in, "0x", 2)) in += 2;
    tmp = in;
    for(len = 0; (tmp - in) < (2*maxlen); tmp++)
    {
	ch[0] = tmp[0];
	ch[1] = tmp[1];
	if(('0' <= ch[0]) && (ch[0] <= 'F') &&
		('0' <= ch[1]) && (ch[1] <= 'F'))
	{
	    out[len++] = strtoul(ch, 0, 16);
	    tmp++;
	}
	else
	    break;
    }
    return len;
}

int double2str(char *buf, int len, double f, char *pr)
{
    unsigned int d1, d2, i;
    double af = (f >= 0) ? f : -f;
    d1 = af;
    double f2 = af - d1;
    double f3 = 1;
    int p = 0, half = 0;
    if(!strncmp(pr, "0.5", 3))
    {
        p = 1;
        half = 1;
    }
    else
        p = str2int(pr);
    if(p <= 0)
    {
        return snprintf(buf, len, "%d", (int)(f + .5));
    }
    if(p >= 6) p = 6;
    for(i = 0; i < p; i++)
    {
        f2 *= 10;
        f3 *= 10;
    }
    if(half == 0)
    {
        f2 += 0.5;
        if(f2 >= f3)
        {
            f2 = 0;
            d1 += 1;
        }
    }
    else
    {
        if((f2 + 2.5) >= f3)
        {
            f2 = 0;
            d1 += 1;
        }
        if((f2 - 2.5) <= (f3 - 10))
            f2 = 0;
        else
            f2 = 5;

    }
    for(d2 = f2; d2 ? !(d2 % 10) : 0; d2 /= 10, p--);
    char fmt[16];
    if(!d2)
    {
        if(f >= 0)
            return snprintf(buf, len, "%u", d1);
        else
            return snprintf(buf, len, "-%u", d1);
    }
    if(f >= 0)
        sprintf(fmt, "%%u.%%.%uu", p);
    else
        sprintf(fmt, "-%%u.%%.%uu", p);
    return snprintf(buf, len, fmt, d1, d2);
}

float interpolate(float x1, float y1, float x2, float y2, float x)
{
    if(x < x1) return y1;
    if(x > x2) return y2;
    return y1 + (x - x1)*(y2 - y1)/(x2 - x1);
}

#if 0
static uint8_t sd_buf[64];
static uint8_t sd_num = 1;
static Thread *sd_threadp = NULL;

void sdwx(uint8_t sd, uint8_t *str, int len)
{
    if(sd == 1)
        sdWrite(&SD1, str, len);
    else if(sd == 2)
        sdWrite(&SD2, str, len);
    else if(sd == 3)
        sdWrite(&SDDBG, str, len);
}

static WORKING_AREA(waSDLoopThread, 128);
static msg_t SDLoopThread(void *p)
{
    (void)p;
    chRegSetThreadName("sd_loop");
    print1("start sd loop");
    uint8_t len = (uint8_t)strlen((char*)sd_buf);
    if(len > sizeof(sd_buf))
	len = sizeof(sd_buf);
    while(!chThdShouldTerminate())
    {
	sdwx(sd_num, sd_buf, len);
	//chThdSleepMilliseconds(100);
    }
    print1("stop sd loop");
    return 0;
}

static void sd_thread(uint8_t start)
{
    if(start)
        sd_threadp = chThdCreateStatic(waSDLoopThread, sizeof(waSDLoopThread), NORMALPRIO, SDLoopThread, NULL);
    else
    {
        if(sd_threadp)
        {
            chThdTerminate(sd_threadp);
            chThdWait(sd_threadp);
        }
        sd_threadp = NULL;
    }
}

void sdw(uint8_t sd, uint8_t *str, int len)
{
    print1("sdw");
    sd_thread(0);
    sdwx(sd, str, len);
}

uint8_t my_sd_read_timeout(SerialDriver *sd, uint8_t *buf, int len, uint8_t end)
{
    int32_t c;
    uint8_t i;
    for(i = 0; i < len; i++)
    {
        c = sdGetTimeout(sd, CH_FREQUENCY);
        print2("sd char", c);
        if((c == Q_TIMEOUT) || (c == Q_RESET) || (c == 0))
            return i;
        if(c == end)
        {
            c = sdGetTimeout(sd, CH_FREQUENCY/5);
            if((c == Q_TIMEOUT) || (c == Q_RESET) || (c == 0))
                return i;
            i = 0;
        }
        buf[i] = c;
    }
    return i;
}

uint8_t sdr(uint8_t sd, uint8_t *buf, int len, uint8_t end)
{
    sd_thread(0);
    if(sd == 1)
        return my_sd_read_timeout(&SD1, buf, len, end);
	//return sdReadTimeout(&SD1, buf, len, CH_FREQUENCY);
    else if(sd == 2)
        return my_sd_read_timeout(&SD2, buf, len, end);
	//return sdReadTimeout(&SD2, buf, len, CH_FREQUENCY);
    else if(sd == 3)
        return my_sd_read_timeout(&SDDBG, buf, len, end);
	//return sdReadTimeout(&SDDBG, buf, len, CH_FREQUENCY);
    return 0;
}

uint8_t sd(uint8_t sd, uint8_t *buf, int len, uint8_t end)
{
    sdw(sd, buf, strlen((char*)buf));
    memset(buf, 0, len);
    return sdr(sd, buf, len, end);
}

void sdloop(uint8_t sd, uint8_t *str, int len)
{
    (void)len;
    sd_thread(0);
    snprintf((char*)sd_buf, sizeof(sd_buf), "%s", (char*)str);
    sd_num = sd;
    sd_thread(1);
}
#endif

#if VFD
int utf8_to_cp866(char *in, char *out, uint32_t maxlen)
{
    uint32_t i, j, k, l = strlen(in);
    uint16_t v;
    out[maxlen - 1] = 0;
    for(i = 0, k = 0; i < l; i++)
    {
	if(k >= (maxlen - 1))
	    break;
	if(!(in[i] >> 7))
	{
	    out[k++] = in[i];
	    continue;
	}
	if((in[i] & 0xC0) == 0xC0)
	{
	    v = ((in[i] << 8) | in[i+1]);
	    i++;
	    for(j = 0; j < sizeof(cp866)/2; j++)
	    {
		if(v == cp866[2*j])
		{
		    out[k++] = (char)cp866[2*j+1];
		    v = 0;
		    break;
		}
	    }
	    if(v)
		break;
	    continue;
	}
	break;
    }
    out[k] = 0;
    return k;
}
#endif

