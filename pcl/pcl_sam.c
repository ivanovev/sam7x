
#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "adc1.h"
#include "gpio.h"
#include "spi1.h"
#include "uart1.h"
#include "efc.h"
#include "util.h"
#include "picol.h"
#include "pcl_sam.h"
#include "uip.h"
#include "events.h"
#include "eth/tftpd.h"

#include <stdio.h>

#if VFD
#include "vfd.h"
#endif

static picolInterp *interp = NULL;
MUTEX_DECL(pcl_mtx);

#ifndef picolSetIntResult
#define picolSetIntResult(i,x)  picolSetFmtResult(i,"%d",x)
#endif

#ifndef picolGetVar
#define   picolGetVar(_i,_n)       picolGetVar2(_i,_n,0)
#endif

extern const char* pcl_str;
extern Thread* maintp;

static int pcl_assert(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 2, "assert i1 i2 i3...");
    int j, ret = 0;
    for(j = 1; j < argc; j++)
    {
	if(!atoi(argv[j]))
	{
	    ret = 1;
	    break;
	}
    }
    if(ret)
    {
	if(i->trace)
	    return picolErr(i,i->ass);
	else
	    return picolErr(i,"assert failed");
    }
    return PICOL_OK;
}
static int pcl_puts(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    if(argc>=2)
        pcl_write_shell(argv[1]);
    return picolSetResult(i,"");
}
static int pcl_mdio(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2(argc >= 2, "mdio reg [value]");
    (void)pd;
    char buf[16];
    unsigned int r, v;
    if(argc >= 2)
        r = str2int(argv[1]);
    if(argc == 2)
    {
        AT91C_BASE_EMAC->EMAC_NCR |= AT91C_EMAC_MPE;
        v = miiGet(&ETHD1, r);
        AT91C_BASE_EMAC->EMAC_NCR &= ~AT91C_EMAC_MPE;
        sprintf(buf, "0x%.4X", v);
    }
    else if(argc == 3)
    {
        v = str2int(argv[2]);
        AT91C_BASE_EMAC->EMAC_NCR |= AT91C_EMAC_MPE;
        miiPut(&ETHD1, r, v);
        AT91C_BASE_EMAC->EMAC_NCR &= ~AT91C_EMAC_MPE;
        sprintf(buf, "0x%.4X", v);
    }
    return picolSetResult(i,buf);
}
static int pcl_pio(picolInterp *i, int argc, char **argv, uint8_t pio)
{
    const char *hlp = "pio[a|b] [per|pdr|psr|asr|bsr|absr|pudr|puer|pusr|oer|odr|osr] ...";
    ARITY2(argc >= 2, hlp);
    char buf[16];
    buf[0] = 0;
    if(argc == 2)
    {
        unsigned int reg = gpio_get_reg(argv[1], pio);
        sprintf(buf, "0x%.8X", reg);
        return picolSetResult(i, buf);
    }
    if(argc == 3)
    {
        gpio_set_reg(argv[1], argv[2], pio);
        return PICOL_OK;
    }
    return picolErr(hlp);
}
static int pcl_pioa(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    return pcl_pio(i, argc, argv, 0);
}
static int pcl_piob(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    return pcl_pio(i, argc, argv, 1);
}
static int pcl_adc(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 2, "adc num [coef]");
    if(argc >= 2)
    {
	char buf[16];
	uint16_t a = adc1_read(atoi(argv[1]));
	if(argc == 2)
	{
	    sprintf(buf, "0x%.4X", a);
	}
	else
	{
	    double f = a*atof(argv[2])/1023.;
	    double2str(buf, sizeof(buf), f, "2");
	}
	return picolSetResult(i, buf);
    }
    return PICOL_ERR;
}
static int pcl_adc_cmp(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 4, "adc_cmp num v1 v2");
    if(argc >= 4)
    {
	uint16_t a = adc1_read(atoi(argv[1]));
	uint16_t v1 = str2int(argv[2]);
	uint16_t v2 = str2int(argv[3]);
	if((a < v1) || (v2 < a))
	    return picolSetIntResult(i, 1);
	return picolSetIntResult(i, 0);
    }
    return PICOL_ERR;
}
static int pcl_gpio(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 2, "gpio num [val]");
#if VERBOSE_IO
    if(argc >= 3)
    {
        char buf[32];
        sprintf(buf, "gpio%s=%s", argv[1], argv[2]);
        print1(buf);
    }
#endif
    if(argc == 2)
    {
        uint8_t gpio = gpio_read(atoi(argv[1]));
        return picolSetIntResult(i, gpio);
    }
    else if(argc >= 3)
    {
        uint8_t n = atoi(argv[1]);
        if(!strncmp(argv[2], "odsr", 4))
            return picolSetIntResult(i, gpio_odsr(n));
        uint8_t val = atoi(argv[2]);
        gpio_write(n, val);
        if(argc > 3)
        {
            if(atoi(argv[3]))
            {
                if(efc_gpio_read(n) != val)
                {
                    efc_gpio_write(n, val);
                    efc_commit("gpio", n);
                }
            }
        }
        return picolSetIntResult(i, val);
    }
    return PICOL_ERR;
}

static int pcl_spi(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 3, "spi n val ...");
    uint8_t nspi = atoi(argv[1]);
    //print2("nspi", nspi);
    uint8_t ncpha, cpol;
    char buf[17];
    uint32_t u32;
    if(!strncmp(argv[2], "ncpha", 5))
    {
        ARITY2(argc >= 3, "spi n ncpha [0|1]");
        spi1_csr_bits(buf, nspi, 1, 1, (argc >= 4) ? str2int(argv[3]) : -1);
	    return picolSetResult(i, buf);
    }
    if(!strncmp(argv[2], "cpol", 4))
    {
        ARITY2(argc >= 3, "spi n cpol [0|1]");
        spi1_csr_bits(buf, nspi, 0, 0, (argc >= 4) ? str2int(argv[3]) : -1);
	    return picolSetResult(i, buf);
    }
    if(!strncmp(argv[2], "bits", 4))
    {
        ARITY2(argc >= 3, "spi n bits ...");
        spi1_csr_bits(buf, nspi, 4, 7, (argc >= 4) ? str2int(argv[3]) : -1);
	    return picolSetResult(i, buf);
    }
    if(!strncmp(argv[2], "scbr", 4))
    {
        ARITY2(argc >= 3, "spi n scbr ...");
        spi1_csr_bits(buf, nspi, 8, 15, (argc >= 4) ? str2int(argv[3]) : -1);
	    return picolSetResult(i, buf);
    }
    if(!strncmp(argv[2], "dlybs", 5))
    {
        ARITY2(argc >= 3, "spi n dlybs ...");
        spi1_csr_bits(buf, nspi, 16, 23, (argc >= 4) ? str2int(argv[3]) : -1);
	    return picolSetResult(i, buf);
    }
    if(!strncmp(argv[2], "dlybct", 5))
    {
        ARITY2(argc >= 3, "spi n dlybct ...");
        spi1_csr_bits(buf, nspi, 24, 31, (argc >= 4) ? str2int(argv[3]) : -1);
	    return picolSetResult(i, buf);
    }
#if 0
    if(!strncmp(argv[2], "rx", 2) || !strncmp(argv[2], "tx", 2))
    {
        ARITY2(argc >= 4, "spi n rx|tx ...");
        u32 = str2int(argv[3]);
        if(!strncmp(argv[2], "rx", 2))
            rx_spi(nspi, u32);
        else
            tx_spi(nspi, u32);
        return picolSetResult(i, argv[3]);
    }
#endif
    if(!strncmp(argv[2], "cr", 2) || !strncmp(argv[2], "mr", 2) || !strncmp(argv[2], "csr", 3))
    {
        u32 = spi1_get_reg(nspi, argv[2]);
        sprintf(buf, "0x%.8X", u32);
        return picolSetResult(i, buf);
    }
    char *data = argv[2];
    if(!strncmp(data, "efc", 3))
    {
        char *val = argv[argc-1];
        char *offset = atoi(argv[argc-2]);
        char dflt[5], efcdata[5];
        int len = str2bytes(val, dflt, 4);
        efc_spi_readstr(efcdata, nspi, offset, len);
        uint32_t efcdatai = strtoul(efcdata, 0, 16);
        if(efcdatai != (0xFFFFFFFF >> 8*(4 - len)))
            data = efcdata;
        else
            data = val;
        argc = argc - 1;
    }
    if(argc >= 3)
    {
        memset(buf, 0, sizeof(buf));
        ncpha = spi1_ncpha(nspi, -1);
        cpol = spi1_cpol(nspi, -1);
        uint8_t loop = 0;
        uint8_t ncpha1 = ncpha;
        uint8_t cpol1 = cpol;
        u32 = 0;
        
        if(argc >= 4)
            loop = !strncmp(argv[argc-1], "loop", 4);
        if(!loop)
        {
            if(argc >= 4)
                cpol1 = (uint8_t)atoi(argv[argc-2]);
            if(argc >= 5)
                ncpha1 = (uint8_t)atoi(argv[argc-3]);
            if(argc >= 6)
            {
                u32 = (uint32_t)atoi(argv[argc-1]);
                if(u32 <= 63)
                    efc_spi_write(nspi, u32, str2int(data));
            }
            if(ncpha1 != ncpha)
                spi1_ncpha(nspi, ncpha1);
            if(cpol1 != cpol)
                spi1_cpol(nspi, cpol1);
        }
        spi1_io(nspi, data, buf, loop);
        if(!loop)
        {
            if(ncpha1 != ncpha)
                spi1_ncpha(nspi, ncpha);
            if(cpol1 != cpol)
                spi1_cpol(nspi, cpol);
        }
        return picolSetResult(i,buf);
    }
    return PICOL_ERR;
}
static int pcl_uart(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2(argc>=2, "uart 0|1|2 str ...");
    (void)pd;
    char buf[64];
    static int32_t timeout = CH_FREQUENCY;
    if(!strncmp(argv[1], "timeout", 7))
    {
        if(argc == 3)
            timeout = (int32_t)str2int(argv[2]);
        return picolSetIntResult(i, timeout);
    }
    uint8_t loop = !strncmp(argv[argc-1], "loop", 4);
    if(loop)
        argc -= 1;
    int len = sizeof(buf), j, k;
    memset(buf, 0, len);
    for(j = 2, k = 0; (j < argc) && (k < (len-2)); j++)
    {
        if(j != 2)
        {
            buf[k++] = ' ';
        }
        k += snprintf(&buf[k], len-k, "%s", argv[j]);
    }
    if(k > 0)
        buf[k++] = '\n';
    buf[k++] = '\0';
    //print1(buf);
    k = uart1_io(atoi(argv[1]), buf, len, timeout, loop, '\n');
    return picolSetResult(i, buf);
}
static int pcl_efc(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 2, "efc gpio|spi ...");
    char buf[33];
    uint32_t tmp1, tmp2;
    uint32_t *ptr;
    float f;
    if(!strncmp(argv[1], "gpio", 4))
    {
        if(argc == 3)
        {
            uint8_t gpio = efc_gpio_read(atoi(argv[2]));
            return picolSetIntResult(i, gpio);
        }
        else if(argc == 4)
        {
            efc_gpio_write(atoi(argv[2]), atoi(argv[3]));
            return picolSetIntResult(i, 0);
        }
        return PICOL_ERR;
    }
    else if(!strncmp(argv[1], "spi", 3))
    {
        if(argc >= 4)
        {
            int spi = atoi(argv[2]);
            uint32_t efcdata;
            int offset;
            char buf1[16];
            if(!strncmp(argv[3], "upd", 3))
            {
                int j;
                for(j = 4; j < argc; j++)
                {
                    offset = atoi(argv[j]);
                    efcdata = efc_spi_read(spi, offset);
                    if(efcdata != 0xFFFFFFFF)
                        efc_spi_write(spi, offset, efcdata);
                }
                sprintf(buf, "%d", argc - 4);
            }
            else
            {
                offset = atoi(argv[3]);
                efcdata = efc_spi_read(spi, offset);
                int len = 4;
                if(argc >= 5)
                {
                    if(strlen(argv[4]) > 1)
                    {
                        len = str2bytes(argv[4], buf1, 4);
                        if(efcdata == 0xFFFFFFFF)
                            return picolSetResult(i, argv[4]);
                    }
                    else
                        len = atoi(argv[4]);
                }
                efc_spi_readstr(buf, spi, offset, len);
            }
            return picolSetResult(i, buf);
        }
        return PICOL_ERR;
    }
    else if(!strncmp(argv[1], "commit", 6))
    {
        int16_t commit_enable = efc_commit_enable(-1);
        if(argc == 2)
        {
            return picolSetResult(i, commit_enable ? "ON" : "OFF");
        }
        if(!strncmp(argv[2], "ON", 2))
        {
            efc_commit_enable(1);
            return picolSetResult(i, "ON");
        }
        if(!strncmp(argv[2], "OFF", 3))
        {
            efc_commit_enable(0);
            return picolSetResult(i, "OFF");
        }
        if((argc < 4) || (commit_enable == 0))
            return PICOL_OK;
        int j;
        for(j = 3; j < argc; j++)
        {
            //print1(argv[j]);
            efc_commit(argv[2], str2int(argv[j]));
        }
	    return picolSetIntResult(i, j-3);
    }
    else if(!strncmp(argv[1], "mr", 2))
    {
        if(argc == 3)
        {
            tmp1 = str2int(argv[2]);
            tmp2 = efc1_readwa(tmp1);
            if(!strncmp(argv[1], "mrf", 3))
            {
                ptr = (uint32_t*)(&f);
                *ptr = tmp2;
                double2str(buf, sizeof(buf), f, "2");
            }
            else
                sprintf(buf, "0x%.8X", tmp2);
            return picolSetResult(i, buf);
        }
        else if(argc == 4)
        {
            uint16_t page = str2int(argv[2]);
            uint8_t offset = str2int(argv[3]);
            sprintf(buf, "0x%.8X", (unsigned int)efc1_readw(page, offset));
            return picolSetResult(i, buf);
        }
        else
            return PICOL_ERR;
    }
    else if(!strncmp(argv[1], "mw", 2))
    {
        if(argc == 4)
        {
            tmp1 = str2int(argv[2]);
            if(!strncmp(argv[1], "mwf", 3))
            {
                f = atof(argv[3]);
                ptr = (uint32_t*)(&f);
                tmp2 = *ptr;
            }
            else
                tmp2 = str2int(argv[3]);
            efc1_writewa(tmp1, tmp2);
            return picolSetResult(i, argv[3]);
        }
        else if(argc == 5)
        {
            uint16_t page = str2int(argv[2]);
            uint8_t offset = str2int(argv[3]);
            efc1_writew(page, offset, str2int(argv[4]));
            return picolSetResult(i, argv[4]);
        }
        else
            return PICOL_ERR;
    }
    else if(!strncmp(argv[1], "fsz", 3))
    {
	    return picolSetIntResult(i, efc1_fsz());
    }
    else if(!strncmp(argv[1], "crc", 3))
    {
        uint32_t n = 1;
        if(argc >= 3)
            n = atoi(argv[2]);
        if(n == 0)
            n = efc0_crc(efc0_fsz());
        else if(n == 1)
            n = efc1_crc(efc1_fsz());
        else
            return PICOL_ERR;
        return picolSetFmtResult(i, "0x%.8X", n);
    }
    else if(!strncmp(argv[1], "ipaddr", 6))
    {
        if(argc >= 3)
        {
            int j, k, ii;
            char *a = argv[2];
            for(j = 0, k = 0, ii = 0; j <= 16; j++)
            {
                if(a[j] == 0)
                {
                    ip_addr.i8[ii++] = atoi(&(a[k]));
                    break;
                }
                if(a[j] == '.')
                {
                    a[j] = 0;
                    ip_addr.i8[ii++] = atoi(&(a[k]));
                    k = j + 1;
                }
            }
            if(ii == 4)
            {
                char macaddr[6];
                efc_macaddr_new();
                efc_ipaddr_write();
                efc_macaddr_write();
                efc_commit("eth", 0);
                return PICOL_OK;
            }
            else
                return PICOL_ERR;
        }
        else
        {
            ipaddr_str(buf, sizeof(buf));
        }
        return picolSetResult(i, buf);
    }
    else if(!strncmp(argv[1], "macaddr", 7))
    {
        efc_macaddr_read();
        uint8_t *ma = mac_addr.i8;
        snprintf(buf, sizeof(buf), "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", ma[0], ma[1], ma[2], ma[3], ma[4], ma[5]);
        return picolSetResult(i, buf);
    }
    else if(SUBCMD("erase"))
    {
        efc1_erase();
        efc_ipaddr_write();
        efc_macaddr_write();
        efc_commit("eth", 0);
        return PICOL_OK;
    }
    return PICOL_ERR;
}
static int pcl_ord(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 2, "ord str");
    char buf[64];
    uint32_t j, len = strlen(argv[1]);
    for(j = 0; j < len; j++)
    {
        if(3*(j + 1) >= sizeof(buf))
            break;
        if(j)
            buf[3*(j % 3) - 1] = ' ';
        sprintf(&(buf[3*j]), "%.2X", argv[1][j]);
    }
    return picolSetResult(i,buf);
}
#if 0
static int pcl_sdw(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2(argc>=3, "sdw 1|2|3 str [loop]");
    (void)pd;
    if(argc == 3)
    {
	sdw(atoi(argv[1]), (uint8_t*)argv[2], strlen(argv[2]));
    }
    else if(argc == 4)
    {
	uint8_t loop = 0;
	if(!strncmp(argv[3], "loop", 4))
	    loop = 1;
	else
	    loop = atoi(argv[3]);
	if(loop)
	    sdloop(atoi(argv[1]), (uint8_t*)argv[2], strlen(argv[2]));
	else
	    sdw(atoi(argv[1]), (uint8_t*)argv[2], strlen(argv[2]));
    }
    return PICOL_OK;
}
static int pcl_sdr(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2(argc>=2, "sdr 1|2|3 [len]");
    (void)pd;
    char buf[32];
    int len = sizeof(buf), l1, j;
    memset(buf, 0, len);
    if(argc == 4)
    {
	l1 = str2int(argv[2]);
	len = (l1 < len) ? l1 : len;
    }
    l1 = sdr(atoi(argv[1]), buf, len, 0);
    return picolSetResult(i, buf);
}
static int pcl_sd(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2(argc>=3, "sd 1|2|3 args");
    (void)pd;
    char buf[64];
    int len = sizeof(buf), j, k;
    memset(buf, 0, len);
    for(j = 2, k = 0; j < argc; j++)
    {
	if(j != 2)
	{
	    buf[k++] = ' ';
	}
	sprintf(&buf[k], "%s", argv[j]);
	k += strlen(argv[j]);
    }
    buf[k] = '\n';
    k = sd(atoi(argv[1]), buf, len, '\n');
    return picolSetResult(i, buf);
}
#endif
#if VFD
static int pcl_cp866(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    char buf[64], buf1[128];
    int j, l;
    ARITY2(argc == 2, "cp866 str");
    l = utf8_to_cp866(argv[1], buf, sizeof(buf));
    sprintf(buf1, "0x");
    for(j = 0; j < l; j++)
    {
	sprintf(&(buf1[2*j+2]), "%.2X", buf[j]);
    }
    return picolSetResult(i,buf1);
}
#if 1
static int pcl_translate(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 3, "translate en ru");
    vfd_translate(argv[1], argv[2]);
    return PICOL_OK;
}
#endif
static int pcl_vfd(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc >= 2, "vfd cls|ctrl|mon|up|down|left|right|br|...");
    if(!strncmp(argv[1], "cls", 3))
    {
	vfd_cls();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "ctrl", 4))
    {
	ARITY2(argc >= 6, "vfd ctrl name type get set ...");
	if(!strncmp(argv[2], "double", 6))
	{
	    ARITY2(argc >= 9, "vfd ctrl double name get set min max step");
	    vfd_add_ctrl_double(argv[3], argv[4], argv[5], atof(argv[6]), atof(argv[7]), atof(argv[8]));
	}
	if(!strncmp(argv[2], "lst", 3))
	{
	    ARITY2(argc >= 8, "vfd ctrl lst name get set var1 var2 ...");
	    int i;
	    for(i = 6; i < argc; i++)
		vfd_add_ctrl_list(argv[3], argv[4], argv[5], argv[i]);
	}
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "mon", 3))
    {
	ARITY2(argc >= 4, "vfd mon name get");
	vfd_add_mon(argv[2], argv[3]);
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "init", 4))
    {
	vfd_menu_init();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "draw", 4))
    {
	vfd_menu_draw();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "up", 2))
    {
	vfd_menu_up();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "down", 4))
    {
	vfd_menu_down();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "left", 4))
    {
	vfd_menu_left();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "right", 5))
    {
	vfd_menu_right();
	return PICOL_OK;
    }
    if(!strncmp(argv[1], "br", 2))
    {
	uint8_t br = vfd_brightness(0);
	if(argc == 2)
	    return picolSetIntResult(i, br);
	return picolSetIntResult(i, vfd_brightness(atoi(argv[2])));
    }
    int j;
    for(j = 1; j < argc; j++)
	vfd_str(argv[j]);
    return PICOL_OK;
}
#endif
static int pcl_temp(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    (void)argc;
    (void)argv;
    char buf[32];
    float temp = 0;
    int8_t nspi = 17;
    if(argc > 1)
        nspi = atoi(argv[1]);
    spi1_get_temp(&temp, nspi);
    double2str(buf, sizeof(buf), temp, "2");
    //sprintf(buf,"%f", temp);
    return picolSetResult(i,buf);
}
static int pcl_sys(picolInterp *i, int argc, char **argv, void *pd)
{
    char buf[MAXSTR];
    buf[0] = 0;
    ARITY2(argc >= 2, "sys ram|version|...");
    if(SUBCMD("ram"))
    {
        return picolSetIntResult(i,chCoreStatus());
    }
    if(SUBCMD("date"))
    {
        version_date_str(buf, sizeof(buf));
        return picolSetResult(i,buf);
    }
    if(SUBCMD("version"))
    {
        version_str(buf, sizeof(buf));
        return picolSetResult(i,buf);
    }
    if(SUBCMD("uptime"))
    {
        uptime_str(buf, sizeof(buf), 0);
        return picolSetResult(i,buf);
    }
    if(SUBCMD("temp"))
    {
        float temp = 0;
        int8_t nspi = 17;
        if(argc > 2)
            nspi = atoi(argv[2]);
        spi1_get_temp(&temp, nspi);
        double2str(buf, sizeof(buf), temp, "2");
        return picolSetResult(i,buf);
    }
    return PICOL_ERR;
}
static int pcl_tftp(picolInterp *i, int argc, char **argv, void *pd)
{
    char buf[MAXSTR];
    buf[0] = 0;
    return PICOL_OK;
}
static int pcl_sleep(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc==2, "sleep ms");
    chThdSleepMilliseconds(atoi(argv[1]));
    return PICOL_OK;
}
static int pcl_int(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2(argc>=2, "int 0x...");
    (void)pd;
    uint32_t u32;
    uint8_t base = 16;
    if(argc == 2)
    {
        u32 = str2int(argv[1]);
        return picolSetIntResult(i,u32);
    }
    else
    {
        base = atoi(argv[2]);
        if(base > 0)
            u32 = strtoul(argv[1], 0, base);
        else
            u32 = str2int(argv[1]);
        return picolSetIntResult(i,u32);
    }
}
static int pcl_hex(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc>=2, "hex int [bytes]");
    (void)pd;
    uint32_t v = str2int(argv[1]);
    uint32_t l = 4;
    if(argc >= 3)
        l = str2int(argv[2]);
    char buf[16];
    if(l == 1)
    {
        v = v & 0xFF;
        sprintf(buf, "0x%.2X", v);
    }
    else if(l == 2)
    {
        v = v & 0xFFFF;
        sprintf(buf, "0x%.4X", v);
    }
    else if(l == 3)
    {
        v = v & 0xFFFFFF;
        sprintf(buf, "0x%.6X", v);
    }
    else
        sprintf(buf, "0x%.8X", v);
    return picolSetResult(i,buf);
}
static int pcl_round(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc>=2, "round float precision");
    double f = atof(argv[1]);
    char *pr = "0";
    if(argc >= 3)
        pr = argv[2];
    char buf[24];
    double2str(buf, sizeof(buf), f, pr);
    return picolSetResult(i, buf);
}
static int pcl_floor(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc>=2, "floor float");
    uint32_t n = atoi(argv[1]);
    return picolSetIntResult(i, n);
}
static int pcl_interpolate(picolInterp *i, int argc, char **argv, void *pd)
{
    ARITY2((argc>=6) && !(argc % 2), "interpolate x1 y1 ... xn yn x");
    (void)pd;
    int j, n = (argc - 4)/2;
    float x, y, x1, y1, x2, y2;
    x1 = atof(argv[1]);
    y1 = atof(argv[2]);
    x = atof(argv[argc - 1]);
    y = atof(argv[argc - 2]);
    for(j = 0; j < n; j++)
    {
	if(x <= x1)
	{
	    y = y1;
	    break;
	}
	x2 = atof(argv[2*j + 3]);
	y2 = atof(argv[2*j + 4]);
	if(x <= x2)
	{
	    y = interpolate(x1, y1, x2, y2, x);
	    break;
	}
	x1 = x2;
	y1 = y2;
    }
    char buf[24];
    double2str(buf, sizeof(buf), y, "6");
    return picolSetResult(i,buf);
}
static int pcl_minmax(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    ARITY2(argc>=3, "min|max val1 val2 ...");
    char buf[16];
    buf[0] = 0;
    double f = 0, f1, f2;
    if(strlen(argv[0] == 3))
    {
        bool_t max = !strncmp(argv[0], "max", 3);
        f = atof(argv[1]);
        int j;
        for(j = 2; j < argc; j++)
        {
            f1 = atof(argv[j]);
            if(max ? (f1 > f) : (f1 < f))
                f = f1;
        }
    }
    if(argc == 4)
    {
        f1 = atof(argv[1]);
        f2 = atof(argv[2]);
        f = atof(argv[3]);
        if(f < f1)
            f = f1;
        else if(f > f2)
            f = f2;
    }
    double2str(buf, sizeof(buf), f, "6");
    return picolSetResult(i, buf);
}
static int pcl_print(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    (void)i;
    if(argc == 2)
    {
        print1(argv[1]);
        return PICOL_OK;
    }
    return PICOL_ERR;
}
static volatile unsigned int timer_dt = 0;
static int pcl_timer(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    if(argc == 1)
        return picolSetIntResult(i, timer_dt/CH_FREQUENCY);
    if(argc == 2)
        timer_dt = (unsigned int)atoi(argv[1])*CH_FREQUENCY;
    return PICOL_OK;
}

#if BTN
extern volatile uint32_t btn_isra, btn_isrb;
static int pcl_btn(picolInterp *i, int argc, char **argv, void *pd)
{
    if(SUBCMD("isra"))
        return picolSetIntResult(i, btn_isra);
    if(SUBCMD("isrb"))
        return picolSetIntResult(i, btn_isrb);
    uint8_t j, btn;
    for(j = 1; j < argc; j++)
    {
        if(argv[j][0] == 'a')
        {
            btn_irq_init(0);
            continue;
        }
        if(argv[j][0] == 'b')
        {
            btn_irq_init(1);
            continue;
        }
        btn = (uint8_t)str2int(argv[j]);
        btn_irq_enable(btn);
    }
    return PICOL_OK;
}
#endif

int pcl_bitMath(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    int a = 0, b = 0, c = -1, p;
    if(argc>=2) a = str2int(argv[1]);
    if(argc==3) b = str2int(argv[2]);
    /*ARITY2 "~ a" */
    if (EQ(argv[0],"~" )) {ARITY(argc==2) c = ~a;}
    /*ARITY2 "& ?arg..." */
    else if (EQ(argv[0],"&" )) {FOLD(c=~0,c &= a,1)}
    /*ARITY2 "| ?arg..." */
    else if (EQ(argv[0],"|" )) {FOLD(c=0,c |= a,1)}
    /*ARITY2 "<< a b" */
    else if (EQ(argv[0],"<<" )) {ARITY(argc==3) c = a << b;}
    /*ARITY2 ">> a b" */
    else if (EQ(argv[0],">>" )) {ARITY(argc==3) c = a >> b;}
    return picolSetIntResult(i,c);
}

#define SCAN_DOUBLE(v,x) {v = atof(x);}
#define FOLD2(init,step,n) {init;for(p=n;p<argc;p++) {SCAN_DOUBLE(a,argv[p]);step;}}

int pcl_doubleMath(picolInterp *i, int argc, char **argv, void *pd)
{
    (void)pd;
    int p;
    double a = 0, b = 0, c = -1;
    if(argc>=2) a = atof(argv[1]);
    if(argc==3) b = atof(argv[2]);
    /*ARITY2 "+ ?arg..." */
    if      (EQ(argv[0],"+" )) {FOLD2(c=0,c += a,1)}
    /*ARITY2 "- arg ?arg...?" */
    else if (EQ(argv[0],"-" )) {if(argc==2) c=-a; else FOLD2(c=a,c -= a,2)}
    /*ARITY2 "* a" */
    else if (EQ(argv[0],"*" )) {FOLD2(c=1,c *= a,1)}
    /*ARITY2 "/ a" */
    else if (EQ(argv[0],"/" )) {ARITY(argc==3) c = a / b;}
    /*ARITY2"> a b" */
    else if (EQ(argv[0],">" )) {ARITY(argc==3) c = a > b;}
    /*ARITY2">= a b"*/
    else if (EQ(argv[0],">=")) {ARITY(argc==3) c = a >= b;}
    /*ARITY2"< a b" */
    else if (EQ(argv[0],"<" )) {ARITY(argc==3) c = a < b;}
    /*ARITY2"<= a b"*/
    else if (EQ(argv[0],"<=")) {ARITY(argc==3) c = a <= b;}
    /*ARITY2"== a b"*/
    else if (EQ(argv[0],"==")) {ARITY(argc==3) c = a == b;}
    /*ARITY2"!= a b"*/
    else if (EQ(argv[0],"!=")) {ARITY(argc==3) c = a != b;}
    char buf[24];
    double2str(buf, sizeof(buf), c, "6");
    return picolSetResult(i, buf);
}

void pcl_init(void)
{
    interp = picolCreateInterp();
    int j;
    char *name1[] = {"~","&","|","<<",">>"};
    for (j = 0; j < (int)(sizeof(name1)/sizeof(char*)); j++)
        picolRegisterCmd(interp,name1[j],pcl_bitMath,NULL);
    char *name2[] = {"+","-","*","/",">",">=","<","<=","==","!="};
    for (j = 0; j < (int)(sizeof(name2)/sizeof(char*)); j++)
        picolRegisterCmd(interp,name2[j],pcl_doubleMath,NULL);
    picolEval(interp, "set DEV \"\"\nproc name {} {global DEV; return $DEV}");
    picolEval(interp, "set DT 3");
    picolEval(interp, "proc boot_cb {} {return 0;}");
    picolEval(interp, "proc timer_cb {} {return 0;}");
    picolRegisterCmd(interp, "adc", pcl_adc, NULL);
    picolRegisterCmd(interp, "adc_cmp", pcl_adc_cmp, NULL);
#if BTN
    picolRegisterCmd(interp, "btn", pcl_btn, NULL);
#endif
    picolRegisterCmd(interp, "assert", pcl_assert, NULL);
    picolRegisterCmd(interp, "floor", pcl_floor, NULL);
    picolRegisterCmd(interp, "gpio", pcl_gpio, NULL);
    picolRegisterCmd(interp, "efc", pcl_efc, NULL);
    picolRegisterCmd(interp, "hex", pcl_hex, NULL);
    picolRegisterCmd(interp, "int", pcl_int, NULL);
    picolRegisterCmd(interp, "interpolate", pcl_interpolate, NULL);
    picolRegisterCmd(interp, "max", pcl_minmax, NULL);
    picolRegisterCmd(interp, "min", pcl_minmax, NULL);
    picolRegisterCmd(interp, "minmax", pcl_minmax, NULL);
    picolRegisterCmd(interp, "ord", pcl_ord, NULL);
    picolRegisterCmd(interp, "pioa", pcl_pioa, NULL);
    picolRegisterCmd(interp, "piob", pcl_piob, NULL);
    picolRegisterCmd(interp, "print", pcl_print, NULL);
    picolRegisterCmd(interp, "puts", pcl_puts, NULL);
    picolRegisterCmd(interp, "mdio", pcl_mdio,NULL);
    picolRegisterCmd(interp, "round", pcl_round, NULL);
    picolRegisterCmd(interp, "sleep", pcl_sleep, NULL);
    picolRegisterCmd(interp, "spi", pcl_spi, NULL);
    picolRegisterCmd(interp, "uart", pcl_uart, NULL);
    picolRegisterCmd(interp, "timer", pcl_timer, NULL);
    picolRegisterCmd(interp, "sys", pcl_sys, NULL);
    picolRegisterCmd(interp, "tftp", pcl_tftp, NULL);
#if 0
    picolRegisterCmd(interp, "sdw", pcl_sdw, NULL);
    picolRegisterCmd(interp, "sdr", pcl_sdr, NULL);
    picolRegisterCmd(interp, "sd", pcl_sd, NULL);
#endif
#if VFD
    picolRegisterCmd(interp, "cp866", pcl_cp866, NULL);
    picolRegisterCmd(interp, "translate", pcl_translate, NULL);
    picolRegisterCmd(interp, "vfd", pcl_vfd, NULL);
#endif
    chMtxInit(&pcl_mtx);
#if VFD
    vfd_menu_init();
#endif
    pcl_load();
}

char* pcl_get_var(char *name)
{
    picolVar *v = (picolVar*)picolGetVar(interp,name);
    if(v)
        return v->val;
    return 0;
}

#if BTN
void pcl_btn_cb(void)
{
    uint16_t j;
    char buf[32];
    sprintf(buf, "btn_cb %d %d", btn_isra, btn_isrb);
    pcl_exec(buf, 0);
}
#endif
void pcl_load(void)
{
    uint8_t buf[MAXSTR];
    int32_t fsz = efc1_fsz();
    int32_t len = 0, rc;
    if(fsz == 0)
        return;
    //print2("rx_status", rx_status());
    uint8_t *fptr = (uint8_t*)EFC1_START;
    pcl_lock();
    while(1)
    {
        while((fptr[0] == 0x0A) && (fsz > 0))
        {
            fptr++;
            fsz--;
        }
        len = pcl_get_chunksz(fptr, fsz);
        //print2("chunk len", len);
        //print5(fptr);
        snprintf(buf, (len < 10) ? len : 10, "%s", fptr);
        if(len == 0)
            break;
        if(len >= sizeof(buf))
        {
            print2("chunk too big", len);
            print2("fsz", fsz);
            print2("fptr", fptr - EFC1_START);
            break;
        }
        memcpy(buf, fptr, len);
        buf[len] = 0;
        rc = picolEval(interp, buf);
        if(rc != PICOL_OK)
        {
            picolVar *v = (picolVar*)picolGetVar(interp,"::errorInfo");
            if(v)
                print4(v->val);
            break;
        }
        fptr += len + 1;
        fsz -= len + 1;
        if(fsz <= 0)
            break;
    }
    print2("loaded sz", fptr - EFC1_START - 1);
    pcl_unlock();
    pcl_exec("boot_cb", 0);
}

void pcl_lock(void)
{
    chMtxLock(&pcl_mtx);
}

int pcl_try_lock(void)
{
    return (chMtxTryLock(&pcl_mtx) ? 1 : 0);
}

void pcl_unlock(void)
{
    chMtxUnlock();
}

void pcl_try_timer_cb(void)
{
    if(!timer_dt)
        return;
    static uint32_t tnow = 0, dt = 0, ts = 0;
    tnow = chTimeNow();
    if((tnow > ts) ? ((tnow - ts) < timer_dt) : (tnow < timer_dt))
        return;
    pcl_exec("timer_cb", 0);
    ts = chTimeNow();
}

int pcl_exec(const char *cmd, char **out)
{
    pcl_lock();
    int ret = picolEval(interp, cmd);
    pcl_unlock();
    if(out)
        *out = interp->result;
    return ret;
}

int pcl_exec2(const char *cmd)
{
    if(pcl_str)
	free((void*)pcl_str);    
    pcl_str = strdup(cmd);
    chEvtSignal(maintp, EVT_CMDSTART);
    eventmask_t m = chEvtWaitAnyTimeout(EVT_PCLOK | EVT_PCLERR, 2000);
    if(m & (EVT_PCLOK | EVT_PCLERR))
    {
        uint32_t ret = m;
        if(ret == EVT_PCLOK)
            ret = 0;
        if(ret == EVT_PCLERR)
            ret = 1;
        shell_reply(ret, pcl_str);
    }
    if(pcl_str)
	free((void*)pcl_str);
    pcl_str = NULL;
    return 0;
}

void pcl_write_shell(char *str)
{
    shell_output(str, "");
}

int pcl_get_line_count(void)
{
    int n = str_line_count(EFC1_START, (0x3FF << 8));
    //print2("line_count", n);
    return n;
}

int32_t pcl_get_chunksz(uint8_t *fptr, int32_t fsz)
{
    uint8_t *ptr1 = (uint8_t*)memchr(fptr, 0x0A, fsz);
    //print2("ptr1[0]", ptr1[0]);
    //print2("ptr1[1]", ptr1[1]);
    if(!ptr1)
        return 0;
    uint32_t len1 = ptr1 - fptr;
    if(ptr1[1] == 0x0A)
        return len1;
    len1 += 1;
    fptr += len1;
    fsz -= len1;
    if(fsz <= 0)
        return len1;
    len1 += pcl_get_chunksz(fptr, fsz);
    return len1;
}

void exit(int status)
{
    (void)status;
    print1("!!!exit!!!");
}

