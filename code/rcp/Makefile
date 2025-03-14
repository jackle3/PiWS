# Include both numbered tests and our specific test file
# PROGS += tests/test_ipv4_header.c
# PROGS += tests/test-rcp-base.c
# PROGS += tests/test-rcp-over-nrf.c
# PROGS += tests/looper.c
PROGS += tests/send_message_single_threaded.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

CFLAGS += -I$(CS140E_PITCP)/code/threads/code
CFLAGS += -I$(CS140E_PITCP)/code/uart

# Common source files
COMMON_SRC := rcp-header.c rcp-datagram.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-driver.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-hw-support.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-public.c

COMMON_SRC += $(CS140E_PITCP)/code/uart/uart-to-tcp.c

# include thread source files
COMMON_SRC += $(CS140E_PITCP)/code/threads/code/eqx-threads.c
COMMON_SRC += $(CS140E_PITCP)/code/threads/code/staff-full-except.c
COMMON_SRC += $(CS140E_PITCP)/code/threads/code/switchto-asm.S
COMMON_SRC += $(CS140E_PITCP)/code/threads/code/full-except-asm.S
STAFF_OBJS += $(CS140E_PITCP)/code/threads/code/staff-breakpoint.o



# Add include paths for NRF
CFLAGS += -I$(CS140E_PITCP)/code/nrf
CFLAGS += -I$(CS140E_PITCP)/code/nrf/tests

STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/staff-hw-spi.o

# we give you a kmalloc
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/kmalloc.o

RUN=1

OUR_START = $(CS140E_PITCP)/code/threads/code/staff-start.S

BOOTLOADER = my-install
EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:\|PASS:\|FAIL:'
include $(CS140E_PITCP)/libpi/mk/Makefile.robust

# Run all tests including our IPv4 header test
test: all
	@echo "Running RCP tests..."
	./tests/test-rcp-base.bin
	./tests/test-rcp-over-nrf.bin

checkoff:
	make -f ./Makefile PROGS="$(wildcard tests/[0123]-*.c)" check
	# make -f ./Makefile PROGS="$(ALL)" check
