###############################################################
# easist to start with.
# PROGS= 1-hello-putk.c
# PROGS= 2-hello-n.c
# PROGS= $(wildcard [01234]-*.c)
# PROGS= 0-hello-get.c
# PROGS= 1-tcp-print.c
# PROGS= 1-serialize-packets.c
PROGS= 2-uart-sender.c

# honestly, once this works, the rest probably do as well.
# PROGS= 0-hello.c

# set to 1 if you want to run.
RUN=0

# Add paths for NRF includes and tests
CFLAGS += -I$(CS140E_PITCP)/code/nrf
CFLAGS += -I$(CS140E_PITCP)/code/nrf/tests
CFLAGS += -I$(CS140E_PITCP)/code/rcp/

COMMON_SRC := sw-uart.c uart-to-tcp.c uart.c
COMMON_SRC += $(CS140E_PITCP)/code/nrf/nrf-hw-support.c $(CS140E_PITCP)/code/nrf/nrf-public.c $(CS140E_PITCP)/code/nrf/nrf-driver.c
COMMON_SRC += $(CS140E_PITCP)/code/rcp/rcp-datagram.c $(CS140E_PITCP)/code/rcp/rcp-header.c



# we give you a kmalloc
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(CS140E_PITCP)/libpi/staff-objs/kmalloc.o

# swap these if your bootloader doesn't work.
BOOTLOADER = my-install

L := $(CS140E_PITCP)/libpi/
# uncomment this if you want to use the staff
# STAFF_OBJS := $(L)/staff-objs/staff-sw-uart.o



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
