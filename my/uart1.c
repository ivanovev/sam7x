
#include "ch.h"
#include "hal.h"
#include "uart1.h"
#include "util.h"

static struct _uart1_msg
{
    uint8_t num;
    char buf[64];
    uint8_t len;
} uart1_msg;

void uart1_init(void)
{
    sdStart(&SD1, NULL);
    sdStart(&SD2, NULL);
    sdStart(&SDDBG, NULL);
}

static void uart1_write(void)
{
    if(uart1_msg.num == 0)
        sdWrite(&SD1, uart1_msg.buf, uart1_msg.len);
    else if(uart1_msg.num == 1)
        sdWrite(&SD2, uart1_msg.buf, uart1_msg.len);
    else if(uart1_msg.num == 2)
        sdWrite(&SDDBG, uart1_msg.buf, uart1_msg.len);
}

uint8_t uart1_read(uint8_t *buf, int len, int32_t timeout, uint8_t endch)
{
    SerialDriver *sd = 0;
    if(uart1_msg.num == 0)
        sd = &SD1;
    else if(uart1_msg.num == 1)
        sd = &SD2;
    else if(uart1_msg.num == 2)
        sd = &SDDBG;
    int32_t c;
    uint8_t i;
    for(i = 0; i < len; i++)
    {   
        c = sdGetTimeout(sd, timeout);
        //print2("sd char", c);
        if((c == Q_TIMEOUT) || (c == Q_RESET) || (c == 0))
            break;
        if(c == endch)
        {   
            c = sdGetTimeout(sd, 1);
            if((c == Q_TIMEOUT) || (c == Q_RESET) || (c == 0))
                break;
            i = 0;
        }
        buf[i] = c;
    }
    buf[i] = 0;
    return i;
}

static WORKING_AREA(waUART1LoopThread, 128);
static msg_t UART1LoopThread(void *p)
{
    (void)p;
    chRegSetThreadName("uart1_loop");
    print1("start uart1 loop");
    while(!chThdShouldTerminate())
    {
        uart1_write();
        //chThdSleepMilliseconds(100);
    }
    print1("stop uart1 loop");
    return 0;
}

int uart1_io(int8_t uartn, char *buf, int8_t len, int32_t timeout, uint8_t loop, char endch)
{
    static Thread *uart1tp = NULL;
    if(uart1tp)
    {   
        chThdTerminate(uart1tp);
        chThdWait(uart1tp);
        uart1tp = NULL;
    }
    uint32_t len1 = strlen(buf);
    len1 = (len1 < len) ? len1 : len;
    if(len1 > sizeof(uart1_msg.buf))
        len1 = sizeof(uart1_msg.buf);
    uart1_msg.num = uartn;
    uart1_msg.len = len1;
    strncpy(uart1_msg.buf, buf, len1);
    if(loop)
    {   
        uart1tp = chThdCreateStatic(waUART1LoopThread, sizeof(waUART1LoopThread), NORMALPRIO, UART1LoopThread, NULL);
        return 0;
    }
    if(len1 > 0)
        uart1_write();
    if(timeout > 0)
        return uart1_read(buf, len, timeout, endch);
    return 0;
}

