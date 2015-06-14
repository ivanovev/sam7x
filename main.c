/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
                 2011,2012 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

                                      ---

    A special exception to the GPL can be applied should you wish to distribute
    a combined work that includes ChibiOS/RT, without being obliged to provide
    the source code for any proprietary components. See the file exception.txt
    for full details of how and when the exception can be applied.
*/

#include "ch.h"
#include "hal.h"
#include "mii.h"
#include "at91sam7_mii.h"

#include "efc.h"
#include "adc1.h"
#include "gpio.h"
#include "spi1.h"
#include "uart1.h"
#include "util.h"
#if PCL
#include "pcl_sam.h"
#if VFD
#include "vfd.h"
#endif
#endif
#include "events.h"

#include "webthread.h"

#include <stdlib.h>
#include <string.h>

static WORKING_AREA(waWebThread, 4096);

Thread *maintp, *wdtp, *webtp;
const char *pcl_str = NULL;

static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *p) {

    (void)p;
    chRegSetThreadName("wdt");
    gpio_led_blink(1);
#if 1
    uint32_t *rmr = (uint32_t*)&(AT91C_BASE_RSTC->RSTC_RMR);
    *rmr = (0xA5 << 24) | (0x8 << 8) | 0x11;
    //uint32_t *per = (uint32_t*)&(AT91C_BASE_PIOA->PIO_PER);
    //*per = 0x19;
#endif
    palClearPad(IOPORT1, 29);
    while(!chThdShouldTerminate()) {
        chThdSleepMilliseconds(100);
        palSetPad(IOPORT1, 29);
        chThdSleepMilliseconds(500);
        palClearPad(IOPORT1, 29);
    }
    palSetPad(IOPORT1, 29);
    //rmr = (uint32_t*)&(AT91C_BASE_RSTC->RSTC_RMR);
    *rmr = (0xA5 << 24);
    return 0;
}

 /*
 * Application entry point.
 */
int main(void) {

    /* * System initializations.
     * - HAL initialization, this also initializes the configured device drivers
     *   and performs the board-specific initializations.
     * - Kernel initialization, the main() function becomes a thread and the
     *   RTOS is active.
     */
    halInit();
    chSysInit();
    gpio_write(29, 1);
    uart1_init();
    wdtp = chThdCreateStatic(waThread1, sizeof(waThread1), HIGHPRIO, Thread1, NULL);

    sdWrite(&SDDBG, (uint8_t *)"boot\r\n", 7);
    adc1_init();
    spi1_init();
    efc_ipaddr_read();
    efc_macaddr_read();
    print_info();
#if 0
    print2("phyid", PHY_ID);
    print2("phyaddr", PHY_ADDRESS);
    print2("phyid1", miiGet(&ETHD1, MII_PHYSID1));
    print2("phyid2", miiGet(&ETHD1, MII_PHYSID2));
    print2("bmsr", miiGet(&ETHD1, MII_BMSR));
#endif
    int8_t erase_img = efc_get_erase_efc1();
    int8_t erase_btn = !palReadPad(IOPORT2, PIOB_SW1);
    if(erase_btn || erase_img)
    {
        print1("erase");
        efc1_erase();
        efc_ipaddr_write();
        efc_macaddr_write();
        efc_commit("eth", 0);
    }
    maintp = chThdSelf();
    pcl_init();
    webtp = chThdCreateStatic(waWebThread, sizeof(waWebThread), LOWPRIO, WebThread, NULL);

    /*
     * Normal main() thread activity.
     */
    //uint32_t ts = 0, tnow, dt = CH_FREQUENCY;
    eventmask_t m;
    eventmask_t waitmask = EVT_CMDSTART | EVT_CMDEND | EVT_PCLUPD | EVT_REBOOT | EVT_FWUPG;
#if VFD
    eventmask_t vfdmask = EVT_VFDN | EVT_VFDS | EVT_VFDW | EVT_VFDE;
    waitmask |= vfdmask;
#endif
    int ret = 0;
    char *out = 0;
    chThdSetPriority(NORMALPRIO);
    volatile int restart_os = 0;
    while(1) {
        pcl_try_timer_cb();
#if VFD
        vfd_menu_try_update();
#endif
        m = chEvtWaitAnyTimeout(waitmask, 500);
        if(m > 0)
        {
            if(m & EVT_CMDSTART)
            {
                if(pcl_str)
                {
                    //print1(pcl_str);
                    ret = pcl_exec(pcl_str, &out);
                    free((void*)pcl_str);
                    pcl_str = strdup(out);
                }
                chEvtSignal(webtp, ret ? EVT_PCLERR : EVT_PCLOK);
            }
            if(m & EVT_PCLUPD)
            {
                pcl_load();
                //shell_cmd();
            }
            if(m & EVT_FWUPG)
            {
                restart_os = 1;
                break;
            }
            if(m & EVT_REBOOT)
            {
                restart_os = 2;
            }
            if(m & EVT_CMDEND)
            {
                if(restart_os)
                    break;
            }
#if VFD
            else if(m & vfdmask)
            {
                if(!vfd_filter_event(m))
                {
                    if(m & EVT_VFDN)
                    vfd_menu_up();
                    else if(m & EVT_VFDS)
                    vfd_menu_down();
                    else if(m & EVT_VFDW)
                    vfd_menu_left();
                    else if(m & EVT_VFDE)
                    vfd_menu_right();
                }
            }
#endif
        }
    }
    //chEvtUnregister(&main_es1, &el1);
    shell_quit(0);
    chThdTerminate(wdtp);
    chThdWait(wdtp);
    chThdSleepMilliseconds(2000);
    chThdTerminate(webtp);
    chThdWait(webtp);
    if(restart_os == 1)
    {
        print1("copy");
        chSysDisable();
        efc10_copy();
    }
    else if(restart_os == 2)
    {
        print1("reboot");
        chSysDisable();
    }

    // HW reset
    uint32_t *ptr1 = (uint32_t*)0xFFFFFD00;
    *ptr1 = 0xA5000001;

    return 0;
}

