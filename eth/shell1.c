 /*
 * Copyright (c) 2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: shell.c,v 1.1 2006/06/07 09:43:54 adam Exp $
 *
 */

#include "ch.h"
#include "hal.h"

#include "shell1.h"

#include <string.h>

#if PCL
#include "picol.h"
#include "pcl_sam.h"
#include "uip.h"
#include "telnetd.h"
#include "util.h"
#include "events.h"
#endif
#ifndef print1
extern void print1(const char *str);
#endif

struct ptentry {
  char *commandstr;
  void (* pfunc)(char *str);
};

#define SHELL_PROMPT "#> "

/*---------------------------------------------------------------------------*/
extern Thread *maintp;

static int parse(register char *str, struct ptentry *t)
{
    struct ptentry *p;
    for(p = t; p->commandstr != NULL; ++p)
    {
        if(strncmp(p->commandstr, str, strlen(p->commandstr)) == 0)
        {
            p->pfunc(str);
            return 0;
        }
    }
    pcl_exec2(str);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
static void
inttostr(register char *str, unsigned int i)
{
  str[0] = '0' + i / 100;
  if(str[0] == '0') {
    str[0] = ' ';
  }
  str[1] = '0' + (i / 10) % 10;
  if(str[0] == ' ' && str[1] == '0') {
    str[1] = ' ';
  }
  str[2] = '0' + i % 10;
  str[3] = ' ';
  str[4] = 0;
}
#endif
/*---------------------------------------------------------------------------*/
static void
help(char *str)
{
    (void)str;
    shell_output("Available commands:", "");
    shell_output("help, ? - show help", "");
    shell_output("exit    - exit shell", "");
}
/*---------------------------------------------------------------------------*/
static void
unknown(char *str)
{
  if(strlen(str) > 0) {
    shell_output("Unknown command: ", str);
  }
}
/*---------------------------------------------------------------------------*/
static void shell_reboot(char *str)
{
    shell_quit(str);
    chEvtSignal(maintp, EVT_REBOOT | EVT_CMDEND);
}
/*---------------------------------------------------------------------------*/
static void shell_exit(char *str)
{
    shell_quit(str);
    chEvtSignal(maintp, EVT_CMDEND);
}
/*---------------------------------------------------------------------------*/
static struct ptentry parsetab[] =
{
    {"help", help},
    {"reboot", shell_reboot},
    {"exit", shell_exit},
    {"?", help},

    /* Default action */
    {NULL, unknown}
};
/*---------------------------------------------------------------------------*/
void
shell_init(void)
{
}
/*---------------------------------------------------------------------------*/
void
shell_start(void)
{
    shell_output("Type '?' and return for help", "");
    shell_input("\n");
}
/*---------------------------------------------------------------------------*/
#if PCL
void shell_cmd(void)
{
    char *name = pcl_get_var("DEV");
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%s", name ? name : "", SHELL_PROMPT);
    shell_prompt(buf);
}
#endif
/*---------------------------------------------------------------------------*/
void
shell_input(char *cmd)
{
    if(cmd[0] == '\n')
    {
        shell_cmd();
    }
    else
    {
        if(parse(cmd, parsetab))
            shell_cmd();
    }
}
/*---------------------------------------------------------------------------*/
void shell_reply(int ret, char *str)
{
    char *out = str;
    //char *bak = out;
    char buf[16];
    sprintf(buf, "[%d] ", ret);
    int len = strlen(buf) + strlen(out);
    if(len < (TELNETD_CONF_LINELEN - 3))
    {
        shell_output(buf, out);
    }
    else
    {
        char buf1[TELNETD_CONF_LINELEN];
        snprintf(buf1, TELNETD_CONF_LINELEN - 2, "%s%s", buf, out);
        shell_output(buf1, "");
        out = out + TELNETD_CONF_LINELEN - strlen(buf) - 3;
        int i;
        for(i = 0; i < TELNETD_CONF_NUMLINES; i++)
        {
            if(!out[0])
                break;
            len = snprintf(buf1, TELNETD_CONF_LINELEN - 2, "%s", out);
            shell_output(buf1, "");
            out += strlen(buf1);
        }
    }
    //free(bak);
    //shell_cmd();
    //print2("tn", chTimeNow());
}

