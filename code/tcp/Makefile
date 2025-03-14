# Include both numbered tests and our specific test file
# PROGS += tests/test-tcp.c
# PROGS += tests/test-tcp-over-nrf.c
# PROGS += tests/test-tcp-retransmit.c
# PROGS += tests/test-tcp-long-messages.c
# PROGS += tests/test-tcp-long-retransmit.c
PROGS += tests/tcp-uart.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

# Common source files
COMMON_SRC := bytestream.c sender.c receiver.c reassembler.c tcp.c

COMMON_SRC += $(CS140E_PITCP)/code/rcp/rcp-header.c $(CS140E_PITCP)/code/rcp/rcp-datagram.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-driver.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-hw-support.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-public.c

COMMON_SRC += $(CS140E_PITCP)/code/uart/uart-to-tcp.c
CFLAGS += -I$(CS140E_PITCP)/code/uart

# Add include paths for NRF
CFLAGS += -I$(CS140E_PITCP)/code/nrf
CFLAGS += -I$(CS140E_PITCP)/code/nrf/tests
CFLAGS += -I$(CS140E_PITCP)/code/rcp


STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/staff-hw-spi.o

# we give you a kmalloc
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/kmalloc.o

RUN=1

BOOTLOADER = my-install
EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:\|PASS:\|FAIL:'
include $(CS140E_PITCP)/libpi/mk/Makefile.robust

# Run all tests
test: all
	@echo "Running TCP tests..."
	./tests/test-tcp.bin

checkoff:
	make -f ./Makefile PROGS="$(wildcard tests/[0123]-*.c)" check