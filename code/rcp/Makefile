# Include both numbered tests and our specific test file
# PROGS += tests/test_ipv4_header.c
PROGS += tests/test-rcp-base.c
PROGS += tests/test-rcp-over-nrf.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

# Common source files
COMMON_SRC := rcp-header.c rcp-datagram.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-driver.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-hw-support.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-public.c

# Add include paths for NRF
CFLAGS += -I$(CS140E_PITCP)/code/nrf
CFLAGS += -I$(CS140E_PITCP)/code/nrf/tests

STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/staff-hw-spi.o

# we give you a kmalloc
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/kmalloc.o

RUN=1

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
