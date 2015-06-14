
#ifndef __VFD_H_
#define __VFD_H_

#define VFD_UP 1 << 27
#define VFD_DOWN 1 << 26
#define VFD_LEFT 1 << 25
#define VFD_RIGHT 1 << 24

void	    vfd_menu_init(void);
void	    vfd_str(const char *str);
void	    vfd_cls(void);
uint8_t	    vfd_brightness(int8_t lvl);
void	    vfd_cp866(void);
void	    vfd_translate(char *en, char *ru);
void	    vfd_add_mon(char *name, char *get);
int8_t	    vfd_menu_try_update(void);
void	    vfd_menu_draw(void);
void	    vfd_menu_up(void);
void	    vfd_menu_down(void);
void	    vfd_menu_left(void);
void	    vfd_menu_right(void);
int8_t	    vfd_filter_event(uint32_t btn);
void	    vfd_add_ctrl_double(char *name, char *get, char *set, double v1, double v2, double step);

#endif
