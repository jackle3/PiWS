// simple mini-uart driver: implement every routine
// with a <todo>.
//
// NOTE:
//  - from broadcom: if you are writing to different
//    devices you MUST use a dev_barrier().
//  - its not always clear when X and Y are different
//    devices.
//  - pay attenton for errata!   there are some serious
//    ones here.  if you have a week free you'd learn
//    alot figuring out what these are (esp hard given
//    the lack of printing) but you'd learn alot, and
//    definitely have new-found respect to the pioneers
//    that worked out the bcm eratta.
//
// historically a problem with writing UART code for
// this class (and for human history) is that when
// things go wrong you can't print since doing so uses
// uart.  thus, debugging is very old school circa
// 1950s, which modern brains arne't built for out of
// the box.   you have two options:
//  1. think hard.  we recommend this.
//  2. use the included bit-banging sw uart routine
//     to print.   this makes things much easier.
//     but if you do make sure you delete it at the
//     end, otherwise your GPIO will be in a bad state.
//
// in either case, in the next part of the lab you'll
// implement bit-banged UART yourself.
#include "rpi.h"

// change "1" to "0" if you want to comment out
// the entire block.
#if 1
//*****************************************************
// We provide a bit-banged version of UART for debugging
// your UART code.  delete when done!
//
// NOTE: if you call <emergency_printk>, it takes
// over the UART GPIO pins (14,15). Thus, your UART
// GPIO initialization will get destroyed.  Do not
// forget!

// header in <libpi/include/sw-uart.h>
#include "sw-uart.h"
static sw_uart_t sw_uart;

// if we've ever called emergency_printk better
// die before returning.
static int called_sw_uart_p = 0;

// a sw-uart putc implementation.
static int sw_uart_putc(int chr) {
    sw_uart_put8(&sw_uart, chr);
    return chr;
}

// call this routine to print stuff.
//
// note the function pointer hack: after you call it
// once can call the regular printk etc.
static void emergency_printk(const char *fmt, ...) {
    // track if we ever called it.
    called_sw_uart_p = 1;

    // we forcibly initialize each time it got called
    // in case the GPIO got reset.
    // setup gpio 14,15 for sw-uart.
    sw_uart = sw_uart_default();

    // all libpi output is via a <putc>
    // function pointer: this installs ours
    // instead of the default
    rpi_putchar_set(sw_uart_putc);

    printk("NOTE: HW UART GPIO is in a bad state now\n");

    // do print
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

#undef todo
#define todo(msg)                                                             \
    do {                                                                      \
        emergency_printk("%s:%d:%s\nDONE!!!\n", __FUNCTION__, __LINE__, msg); \
        rpi_reboot();                                                         \
    } while (0)

// END of the bit bang code.
#endif

enum {
    AUX_ENABLES = 0x20215004,      // used to enable/disable the uart
    AUX_MU_IO_REG = 0x20215040,    // used to send via tx and receive via rx
    AUX_MU_IER_REG = 0x20215044,   // used to enable/disable interrupts
    AUX_MU_IIR_REG = 0x20215048,   // used to clear tx and rx FIFOs
    AUX_MU_LCR_REG = 0x2021504C,   // used for data size (7 or 8 bits)
    AUX_MU_CNTL_REG = 0x20215060,  // used to enable/disable tx and rx
    AUX_MU_STAT_REG = 0x20215064,  // used for transmitter done, space available
    AUX_MU_BAUD_REG = 0x20215068,  // used to set the baud rate
};

//*****************************************************
// the rest you should implement.

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    // NOTE: make sure you delete all print calls when
    // done!

    // perhaps confusingly: at this point normal printk works
    // since we overrode the system putc routine.
    // printk("write UART addresses in order\n");
    dev_barrier();

    // 1. set up the tx and rx gpio pins
    gpio_set_function(GPIO_TX, GPIO_FUNC_ALT5);  // set tx to output (to send)
    gpio_set_function(GPIO_RX, GPIO_FUNC_ALT5);  // set rx to input (to receive)
    dev_barrier();

    // 2. turn on the uart in aux
    uint32_t aux_enables = GET32(AUX_ENABLES);
    PUT32(AUX_ENABLES, aux_enables | 1);  // p9
    dev_barrier();

    // 3. immediately disable tx and rx FIFOs
    PUT32(AUX_MU_CNTL_REG, 0);  // p17

    // 4. clear out state, including the FIFOs
    PUT32(AUX_MU_IIR_REG, 0b110);  // p13

    // 5. disable uart interrupts
    PUT32(AUX_MU_IER_REG, 0);  // p12

    // 6. configure the baud rate to 115200 Baud, 8 bits, 1 start bit, 1 stop bit
    PUT32(AUX_MU_LCR_REG, 0b11);  // p14, set to 8-bit mode
    PUT32(AUX_MU_BAUD_REG, 270);  // p19, 115200 baud for a 250 MHz clock

    // 7. enable the tx and rx FIFOs
    PUT32(AUX_MU_CNTL_REG, 0b11);  // p17

    dev_barrier();

    // delete everything to do w/ sw-uart when done since
    // it trashes your hardware state and the system
    // <putc>.
    demand(!called_sw_uart_p, "delete all sw-uart uses or hw UART in bad state");
}

// disable the uart: make sure all bytes have been
// transmitted before disabling the uart
void uart_disable(void) {
    dev_barrier();

    uart_flush_tx();
    uint32_t aux_enables = GET32(AUX_ENABLES);
    PUT32(AUX_ENABLES, aux_enables & ~1);  // p9

    dev_barrier();
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    // bit 0 is 1 if there is data in the FIFO
    return GET32(AUX_MU_STAT_REG) & (1 << 0);
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is
// at least one byte.
int uart_get8(void) {
    dev_barrier();

    // bit 0 of AUX_MU_STAT_REG is 1 if there is data in the FIFO
    while (!uart_has_data()) {
        continue;
    }
    uint32_t v = GET32(AUX_MU_IO_REG) & 0xFF;

    dev_barrier();
    return v;
}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    // bit 1 is 1 if there is space in the FIFO
    return GET32(AUX_MU_STAT_REG) & (1 << 1);
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    dev_barrier();

    while (!uart_can_put8()) {
        continue;
    }
    PUT32(AUX_MU_IO_REG, c);

    dev_barrier();
    return 0;
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) {
    dev_barrier();

    if (!uart_has_data())
        return -1;
    uint8_t v = uart_get8();

    dev_barrier();
    return v;
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    dev_barrier();

    // p18: bit 9 (transmitter done) is 1 if the FIFO is empty
    // and the transmitter is idle
    uint32_t v = GET32(AUX_MU_STAT_REG) & (1 << 9);

    dev_barrier();
    return v;
}

// return only when the TX FIFO is empty AND the
// TX transmitter is idle.
//
// used when rebooting or turning off the UART to
// make sure that any output has been completely
// transmitted.  otherwise can get truncated
// if reboot happens before all bytes have been
// received.
void uart_flush_tx(void) {
    dev_barrier();

    while (!uart_tx_is_empty()) rpi_wait();

    dev_barrier();
}
