# Include both numbered tests and our specific test file
PROGS= 2-uart-recv.c

LIBS += $(CS140E_PITCP)/lib/libgcc.a

# Common source files
COMMON_SRC := sw-uart.c uart-to-tcp.c uart.c

COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-hw-support.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-public.c 
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-driver.c
COMMON_SRC += $(CS140E_PITCP)/code/rcp/rcp-datagram.c
COMMON_SRC += $(CS140E_PITCP)/code/rcp/rcp-header.c

# Add include paths for NRF
CFLAGS += -I$(CS140E_PITCP)/code/nrf
CFLAGS += -I$(CS140E_PITCP)/code/nrf/tests
CFLAGS += -I$(CS140E_PITCP)/code/rcp

STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/staff-hw-spi.o

# we give you a kmalloc
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/kmalloc.o

RUN=0

BOOTLOADER = my-install

# if doing checkoff use both uart and cstart.
ifdef CHECKOFF
    COMMON_SRC = sw-uart.c
    PROGS= $(wildcard [01234]-*.c)
endif

include $(CS140E_PITCP)/libpi/mk/Makefile.robust

checkoff:
	make -C ./ CHECKOFF=1 check
	@echo "checking that not using staff uart: if you see output is an error:"
	arm-none-eabi-nm $(BUILD_DIR)/*.elf  | grep staff_uart
