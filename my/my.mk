
MYSRC = my/adc1.c \
	my/gpio.c \
	my/efc.c \
	my/spi1.c \
	my/uart1.c \
	my/util.c

MYINC = my

all: MAKE_ALL_RULE_HOOK

MAKE_ALL_RULE_HOOK:
	touch my/util.c
