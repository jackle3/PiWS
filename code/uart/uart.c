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
    sw_uart_put8(&sw_uart,chr);
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
#define todo(msg) do {                      \
    emergency_printk("%s:%d:%s\nDONE!!!\n",      \
            __FUNCTION__,__LINE__,msg);   \
    rpi_reboot();                           \
} while(0)

// END of the bit bang code.
#endif


//*****************************************************
// the rest you should implement.

enum UART_REG {
    AUX_EN = 0x20215004,
    AUX_MU_IO = 0x20215040,
    AUX_MU_IER = 0x20215044,
    AUX_MU_IIR = 0x20215048,
    AUX_MU_LCR = 0x2021504C,
    AUX_MU_CNTL = 0x20215060,
    AUX_MU_STAT = 0x20215064,
    AUX_MU_BAUD = 0x20215068
};


// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    dev_barrier();
    // NOTE: make sure you delete all print calls when
    // done!

    // perhaps confusingly: at this point normal printk works
    // since we overrode the system putc routine.
    //printk("write UART addresses in order\n");

    // 0. Set up GPIO pin 14 (TX), 15 (RX)
    gpio_set_function(14, GPIO_FUNC_ALT5);
    gpio_set_function(15, GPIO_FUNC_ALT5);

    dev_barrier();

    // 1. Enable MiniUART
    uint32_t read_en = GET32(AUX_EN);
    read_en |= 0b1;
    PUT32(AUX_EN, read_en);

    dev_barrier();

    // 2. Disable TX/RX
    PUT32(AUX_MU_CNTL, 0x0);

    // 4. Disable UART interrupts
    PUT32(AUX_MU_IER, 0x0);

    // 3. Clear TX/RX
    PUT32(AUX_MU_IIR, 0x6);

    // 5. Set data size - 8 bit
    PUT32(AUX_MU_LCR, 0b11);

    // 6. Set Baud rate - 115200 ish (set 270)
    PUT32(AUX_MU_BAUD, 270);

    // 7. Reenable TX/RX, should be ready here!
    PUT32(AUX_MU_CNTL, 0b11);

    // delete everything to do w/ sw-uart when done since
    // it trashes your hardware state and the system
    // <putc>.
    demand(!called_sw_uart_p, 
        delete all sw-uart uses or hw UART in bad state);
    dev_barrier();
}

// disable the uart: make sure all bytes have been
// 
void uart_disable(void) {
    dev_barrier();
    uart_flush_tx();
    uint32_t read_en = GET32(AUX_EN);
    read_en &= ~0b1;
    PUT32(AUX_EN, read_en);
    dev_barrier();
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is 
// at least one byte.
int uart_get8(void) {
    dev_barrier();
    while(!uart_has_data())
        rpi_wait();
    uint32_t byte = GET32(AUX_MU_IO);
    dev_barrier();
    return byte & 0xFF;
}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    uint32_t has_space = GET32(AUX_MU_STAT);
    return (has_space & 0b10) == 0b10;
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    dev_barrier();
    while(!uart_can_put8()) {
        rpi_wait();
    }
    uint32_t write = c;
    PUT32(AUX_MU_IO, write);
    dev_barrier();
    return 0;
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    __int32_t has_data = GET32(AUX_MU_STAT) & 0b1;
    return has_data;
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) { 
    if(!uart_has_data())
        return -1;
    return uart_get8();
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    __uint32_t stats = GET32(AUX_MU_STAT);
    // Check transmitter done bit
    return (stats & 0b1000000000) == 0b1000000000;    
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
    while(!uart_tx_is_empty())
        rpi_wait();
    dev_barrier();
}
