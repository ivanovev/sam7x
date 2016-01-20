
#include "ch.h"
#include "hal.h"
#include "vfd.h"
#include "btn_irq.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#ifdef PCL
#include "picol.h"
#include "pcl_sam.h"
#endif

typedef struct vfd_menu_state {
    uint8_t lvl, scroll;
} vfd_menu_state;

typedef struct vfd_menu_edit_double {
    double min, max, cur, step;
} vfd_menu_edit_double;

typedef struct vfd_menu_edit_list {
    uint8_t count, cur;
    char **data;
} vfd_menu_edit_list;

#define VFD_EDIT_DOUBLE 1
#define VFD_EDIT_LIST 2

typedef struct vfd_menu_edit {
    char *set;
    uint8_t editing;
    uint8_t type;
    uint8_t immediate;
    union {
	vfd_menu_edit_double d_f;
	vfd_menu_edit_list d_l;
    } data;
} vfd_menu_edit;

typedef struct vfd_menu_item {
    char *name;
    char *get;
    vfd_menu_edit *edit;
    void *sub;
} vfd_menu_item;

typedef struct vfd_menu_item_list {
    uint8_t count;
    int8_t sel;
    vfd_menu_item **items;
} vfd_menu_item_list;

enum vfd_main_menu
{
    MENU_CTRL,
    MENU_MON,
    MENU_SYS
};

typedef struct vfd_words {
    char **en_ru;
    int len;
} vfd_words;

static vfd_menu_state *pstate = NULL;
static vfd_menu_item_list *pmain = NULL;
static vfd_words *pwords = NULL;

uint32_t vfd_dt = 2*CH_FREQUENCY;

#define MENU_IPADDR "IP-Address"
#define MENU_VERSION "Version"
#define MENU_BRIGHTNESS "Brightness"

int8_t vfd_filter_event(uint32_t btn)
{
    static uint32_t tnow = 0, ts = 0;
    static uint32_t btn_prev = 0;
    tnow = chTimeNow();
    if((tnow > ts) ? ((tnow - ts) > vfd_dt) : (tnow > vfd_dt))
	btn_prev = 0;
    ts = tnow;
    if(btn == btn_prev)
    {
	return 1;
    }
    btn_prev = btn;
    return 0;
}

uint8_t vfd_brightness(int8_t newlvl)
{
    static uint8_t lvl = 3;
    if((1 <= newlvl) && (newlvl <= 8))
	lvl = newlvl;
    else
	return lvl;
    char buf[16];
    sprintf(buf, "0x1F58%.2X", lvl);
    vfd_str(buf);
    return lvl;
}

static vfd_menu_item* vfd_menu_append_(vfd_menu_item_list *list, char *name, char *get)
{
    vfd_menu_item *item = calloc(1, sizeof(vfd_menu_item));
    item->name = strdup(name);
    if(get)
	item->get = strdup(get);
    list->count++;
    list->items = realloc(list->items, list->count*(sizeof(vfd_menu_item*)));
    list->items[list->count-1] = item;
    return item;
}

static void vfd_menu_make_edit_double(vfd_menu_item* item, char *set, double v1, double v2, double step, uint8_t imm)
{
    item->edit = calloc(1, sizeof(vfd_menu_edit));
    item->edit->set = strdup(set);
    item->edit->data.d_f.min = v1;
    item->edit->data.d_f.max = v2;
    item->edit->data.d_f.step = step;
    item->edit->type = VFD_EDIT_DOUBLE;
    item->edit->immediate = imm;
    char *tmp;
    int ret = pcl_exec(item->get, &tmp);
    if(ret == PICOL_OK)
	item->edit->data.d_f.cur = atof(tmp);
}

static void vfd_menu_make_edit_list(vfd_menu_item* item, char *set)
{
    item->edit = calloc(1, sizeof(vfd_menu_edit));
    item->edit->set = strdup(set);
    item->edit->type = VFD_EDIT_LIST;
    char *tmp;
    int ret = pcl_exec(item->get, &tmp);
    if(ret == PICOL_OK)
	item->edit->data.d_l.cur = atoi(tmp);
}

static vfd_menu_item* vfd_menu_find_edit_list(char *name, char *get, char *set)
{
    if(!pmain->items[MENU_CTRL]->sub)
	pmain->items[MENU_CTRL]->sub = calloc(1, sizeof(vfd_menu_item_list*));
    vfd_menu_item_list *list = pmain->items[MENU_CTRL]->sub;
    int i;
    for(i = 0; i < list->count; i++)
    {
	if(!strcmp(list->items[i]->name, name))
	    return list->items[i];
    }
    vfd_menu_item* item = vfd_menu_append_(pmain->items[MENU_CTRL]->sub, name, get);
    vfd_menu_make_edit_list(item, set);
    return item;
}

static vfd_menu_item* vfd_menu_get(vfd_menu_item_list *list, uint8_t lvl)
{
    if(!list)
	list = pmain;
    if(!list->items)
	return NULL;
    if(list->sel < 0)
	return NULL;
    if(list->sel >= list->count)
	return NULL;
    if(lvl == 0)
	return list->items[list->sel];
    return vfd_menu_get((vfd_menu_item_list*)list->items[list->sel]->sub, lvl-1);
}

static void vfd_reverse(uint8_t r)
{
    char buf[16];
    sprintf(buf, "0x1F72%.2X", r);
    vfd_str(buf);
}

static vfd_menu_item *vfd_add_menu_item(uint8_t id, char *name, char *get)
{
    if(!pmain->items[id]->sub)
	pmain->items[id]->sub = calloc(1, sizeof(vfd_menu_item_list*));
    return vfd_menu_append_((vfd_menu_item_list*)pmain->items[id]->sub, name, get);
}

void vfd_add_mon(char *name, char *get)
{
    vfd_add_menu_item(MENU_MON, name, get);
}

void vfd_add_ctrl_double(char *name, char *get, char *set, double v1, double v2, double step)
{
    vfd_menu_item *item = vfd_add_menu_item(MENU_CTRL, name, get);
    vfd_menu_make_edit_double(item, set, v1, v2, step, 0);
}

void vfd_add_ctrl_list(char *name, char *get, char *set, char *val)
{
    vfd_menu_item *item = vfd_menu_find_edit_list(name, get, set);
    if(!item)
	return;
    if(!item->edit->set)
	item->edit->set = strdup(set);
    
    item->edit->data.d_l.count++;
    if(!item->edit->data.d_l.data)
	item->edit->data.d_l.data = calloc(1, sizeof(char*));
    else
	item->edit->data.d_l.data = realloc(item->edit->data.d_l.data, item->edit->data.d_l.count*sizeof(char*));
    item->edit->data.d_l.data[item->edit->data.d_l.count-1] = strdup(val);
}

void vfd_menu_init(void)
{
    if(pmain)
	return;
    
    vfd_menu_item *item;
    pmain = calloc(1, sizeof(vfd_menu_item_list));
    pstate = calloc(1, sizeof(vfd_menu_state));
    vfd_menu_append_(pmain, "Control", 0);
    vfd_menu_append_(pmain, "Monitoring", 0);
    vfd_menu_append_(pmain, "System", 0);
    pmain->items[MENU_SYS]->sub = calloc(1, sizeof(vfd_menu_item_list*));
    vfd_menu_append_((vfd_menu_item_list*)pmain->items[MENU_SYS]->sub, MENU_IPADDR, "ipaddr");
    vfd_menu_append_((vfd_menu_item_list*)pmain->items[MENU_SYS]->sub, MENU_VERSION, "build date");
    item = vfd_menu_append_((vfd_menu_item_list*)pmain->items[MENU_SYS]->sub, MENU_BRIGHTNESS, "vfd br");
    vfd_menu_make_edit_double(item, "vfd br", 1, 8, 1, 1);
    vfd_cls();
    vfd_cp866();
    vfd_brightness(3);
    vfd_menu_draw();
    vfd_irq_init();
}

void vfd_str(const char *str)
{
    int j;
    if(pwords)
    {
	for(j = 0; j < pwords->len/2; j++)
	{
	    if(!strcmp(str, pwords->en_ru[2*j]))
	    {
		str = pwords->en_ru[2*j+1];
		break;
	    }
	}
    }
    uint8_t tx[32];
    if(!strncmp(str, "0x", 2))
    {
	int len = str2bytes(str, tx, sizeof(tx));
	sdw(1, tx, len);
	return;
    }
    sdw(1, (uint8_t*)str, strlen(str));
}

void vfd_cls(void)
{
    vfd_str("0x0C");
}

void vfd_cp866(void)
{
    vfd_str("0x1B7411");
}

void vfd_translate(char *en, char *ru)
{
    if(!pwords)
	pwords = calloc(1, sizeof(vfd_words));
    pwords->len += 2;
    if(!pwords->en_ru)
	pwords->en_ru = calloc(pwords->len, sizeof(char*));
    else
	pwords->en_ru = realloc(pwords->en_ru, pwords->len*sizeof(char*));
    pwords->en_ru[pwords->len - 2] = strdup(en);
    pwords->en_ru[pwords->len - 1] = strdup(ru);
}

void vfd_menu_draw_mark(char *m)
{
    vfd_str("0x1F2468000100");
    vfd_str(m);
}

static void vfd_menu_change_double_val(vfd_menu_item *item, int8_t step, int8_t imm)
{
    double f = item->edit->data.d_f.cur;
    f += step*item->edit->data.d_f.step;
    char buf1[16], buf[64];
    if((item->edit->data.d_f.min <= f) && (f <= item->edit->data.d_f.max))
    {
	if(imm)
	{
	    char *tmp;
	    double2str(buf1, sizeof(buf1), f, "2");
	    snprintf(buf, sizeof(buf), "%s %s", item->edit->set, buf1);
	    pcl_exec(buf, &tmp);
	}
	item->edit->data.d_f.cur = f;
    }
    double2str(buf, sizeof(buf), item->edit->data.d_f.cur, "2");
    vfd_str(buf);
}

static void vfd_menu_change_list_val(vfd_menu_item *item, int8_t step, int8_t imm)
{
    int i = item->edit->data.d_l.cur;
    i += step;
    char buf[64];
    if((0 <= i) && (i < item->edit->data.d_l.count))
    {
	if(imm)
	{
	    char *tmp;
	    snprintf(buf, sizeof(buf), "%s %i", item->edit->set, i);
	    pcl_exec(buf, &tmp);
	}
	item->edit->data.d_l.cur = i;
    }
    vfd_str(item->edit->data.d_l.data[item->edit->data.d_l.cur]);
}

static void vfd_menu_change_val(int8_t step, int8_t imm)
{
    vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
    if(!item)
	return;
    if(!item->get)
	return;
    if(!item->edit->set)
	return;
    vfd_str("0x0D");
    if(item->edit->type == VFD_EDIT_DOUBLE)
    {
	vfd_menu_change_double_val(item, step, imm);
    }
    if(item->edit->type == VFD_EDIT_LIST)
    {
	vfd_menu_change_list_val(item, step, imm);
    }
    vfd_str("   ");
    if(item->edit)
    {
	if(item->edit->editing)
	    vfd_menu_draw_mark(imm ? "=" : "*");
	else
	    vfd_menu_draw_mark(" ");
    }
}

static void vfd_menu_update_val(void)
{
    vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
    if(!item)
	return;
    if(!item->get)
	return;
    char *out;
    vfd_str("0x0D");
    int ret = pcl_exec(item->get, &out);
    if(ret != PICOL_OK)
	vfd_str("Error");
    else
    {
	if(item->edit ? (item->edit->type == VFD_EDIT_LIST) : 0)
	{
	    int i = atoi(out);
	    if((0 <= i) && (i < item->edit->data.d_l.count))
	    {
		vfd_str(item->edit->data.d_l.data[i]);
		item->edit->data.d_l.cur = i;
	    }
	    else
		vfd_str("Error");
	}
	else
	    vfd_str(out);
    }
    vfd_str("   ");
    if(item->edit)
    {
	if(item->edit->editing)
	    vfd_menu_draw_mark(item->edit->immediate ? "=" : "*");
	else
	    item->edit->data.d_f.cur = atof(out);
    }
}

int8_t vfd_menu_try_update(void)
{
#ifdef PCL
    if(pmain->sel != MENU_MON)
	return 1;
    if(pstate->lvl == 0)
	return 1;
    static uint32_t ts = 0;
    uint32_t tnow = chTimeNow();
    uint32_t dt = vfd_dt;
    if((tnow > ts) ? ((tnow - ts) < dt) : (tnow < dt))
        return 0;
    vfd_menu_update_val();
    ts = chTimeNow();
#endif
    return 0;
}

void vfd_menu_draw(void)
{
    vfd_cls();
    if(pstate->lvl == 0)
    {
	uint8_t i;
	uint8_t maxsel = pmain->count;
	for(i = pstate->scroll; (i < maxsel) && (i <= pstate->scroll + 1); i++)
	{
	    if(pmain->sel == i)
		vfd_reverse(1);
	    vfd_str(pmain->items[i]->name);
	    vfd_reverse(0);
	    if(i == pstate->scroll)
		vfd_str("\n\r");
	}
	return;
    }
    if(pstate->lvl == 1)
    {
	vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
	if(!item)
	    return;
	vfd_str(item->name);
	vfd_str("\n\r");
	vfd_menu_update_val();
    }
}

static vfd_menu_item_list *vfd_menu_get_items(void)
{
    vfd_menu_item_list *list = NULL;
    if(pstate->lvl == 0)
	list = pmain;
    if(pstate->lvl == 1)
	list = (vfd_menu_item_list*)pmain->items[pmain->sel]->sub;
    return list;
}

void vfd_menu_up(void)
{
    vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
    uint8_t done = 0;
    if(item)
    {
	if(item->edit)
	{
	    if(item->edit->editing)
	    {
		vfd_menu_change_val(1, item->edit->immediate);
		return;
	    }
	}
    }
    if(!done)
    {
	vfd_menu_item_list *list = vfd_menu_get_items();
	if(list->sel > 0)
	    list->sel--;
	if(pstate->lvl == 0)
	{
	    if(pstate->scroll > pmain->sel)
		pstate->scroll--;
	}
    }
    vfd_menu_draw();
}

void vfd_menu_down(void)
{
    vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
    uint8_t done = 0;
    if(item)
    {
	if(item->edit)
	{
	    if(item->edit->editing)
	    {
		vfd_menu_change_val(-1, item->edit->immediate);
		return;
	    }
	}
    }
    if(!done)
    {
	vfd_menu_item_list *list = vfd_menu_get_items();
	uint8_t maxsel = list->count - 1;
	if(list->sel < maxsel)
	    list->sel++;
	if(pstate->lvl == 0)
	{
	    if(pmain->sel > (pstate->scroll + 1))
		pstate->scroll++;
	}
    }
    vfd_menu_draw();
}

void vfd_menu_left(void)
{
    vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
    uint8_t done = 0;
    if(item)
    {
	if(item->edit)
	{
	    if(item->edit->editing)
	    {
		item->edit->editing = 0;
		done = 1;
	    }
	}
    }
    if(!done)
    {
	if(pstate->lvl > 0)
	    pstate->lvl--;
    }
    vfd_menu_draw();
}

void vfd_menu_right(void)
{
    vfd_menu_item *item = vfd_menu_get(NULL, pstate->lvl);
    if(item)
    {
	if(item->sub)
	{
	    vfd_menu_item_list *list = (vfd_menu_item_list*)item->sub;
	    list->sel = 0;
	    pstate->lvl++;
	}
	if(item->edit)
	{
	    if(!item->edit->editing)
		item->edit->editing = 1;
	    else
	    {
		item->edit->editing = 0;
		vfd_menu_change_val(0, 1);
		return;
	    }
	}
    }
    vfd_menu_draw();
}

