# PROGS += $(wildcard tests/[0123456]-*.c)
PROGS += tests/4-one-way-ack-32bytes.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

COMMON_SRC := nrf-hw-support.c nrf-public.c nrf-driver.c

STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/kmalloc.o

RUN=1

BOOTLOADER = my-install
EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:'
include $(CS140E_PITCP)/libpi/mk/Makefile.robust

checkoff:
	make -f ./Makefile PROGS="$(wildcard tests/[0123]-*.c)" check
	# make -f ./Makefile PROGS="$(ALL)" check
