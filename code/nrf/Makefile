# PROGS += $(wildcard tests/[0123]-*.c)
# PROGS := tests/2-ping-pong-ack-4bytes.c
PROGS := tests/5-multi-pipe.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

COMMON_SRC := nrf-hw-support.c nrf-public.c nrf-driver.c #my_spi.c
# COMMON_SRC += $(CS140E_PITCP)/code/rcp/rcp-header.c $(CS140E_PITCP)/code/rcp/rcp-datagram.c

# # Add RCP include path
# CFLAGS += -I$(CS140E_PITCP)/code/rcp

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
