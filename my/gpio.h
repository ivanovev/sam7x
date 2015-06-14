
#ifndef __GPIO_H_
#define __GPIO_H_

void	gpio_configure_pins(void);
//void	gpio_restore_saved(void);
void	gpio_write(uint8_t gpio, uint8_t val);
uint8_t	gpio_read(uint8_t gpio);
uint8_t gpio_odsr(uint8_t n);
void	gpio_input_all(void);
void	gpio_led_on(void);
void	gpio_led_off(void);
void	gpio_led_blink(uint8_t n);
uint32_t gpio_get_reg(char *name, uint8_t pio);
void	gpio_set_reg(char *name, char *val, uint8_t pio);

#endif
