# Include both numbered tests and our specific test file
# PROGS += tests/test_ipv4_header.c
PROGS += tests/test_ipv4_datagram.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

# Common source files
COMMON_SRC := ipv4_header.c ipv4_datagram.c

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
	@echo "Running IPv4 header tests..."
	./tests/test_ipv4_header.bin

checkoff:
	make -f ./Makefile PROGS="$(wildcard tests/[0123]-*.c)" check
	# make -f ./Makefile PROGS="$(ALL)" check
