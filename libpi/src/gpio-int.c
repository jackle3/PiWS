// engler, cs140 put your gpio-int implementations in here.
#include "rpi.h"

// in libpi/include: has useful enums.
#include "rpi-interrupts.h"

enum {
    GPEDS0 = 0x20200040, // p96, edge event detected
    GPREN0 = 0x2020004C, // p97, enable rising edge detection
    GPFEN0 = 0x20200058, // p98, enable falling edge detection
};

#define GPIO_INT0_IRQ 17 // bit 17 of IRQ_xxx_2 is for IRQ 49 (GPIO_INT 0)

// returns 1 if there is currently a GPIO_INT0 interrupt, 
// 0 otherwise.
//
// note: we can only get interrupts for <GPIO_INT0> since the
// (the other pins are inaccessible for external devices).
int gpio_has_interrupt(void) {
    // p115, indicates which IRQs (from 32 to 63) are pending
    dev_barrier();
    uint32_t pending = GET32(IRQ_pending_2);
    dev_barrier();

    // p113, gpio_int[0] is at IRQ 49, so bit 17 of pending
    return (pending & (1 << GPIO_INT0_IRQ)) != 0;
}

// p97 set to detect rising edge (0->1) on <pin>.
// as the broadcom doc states, it  detects by sampling based on the clock.
// it looks for "011" (low, hi, hi) to suppress noise.  i.e., its triggered only
// *after* a 1 reading has been sampled twice, so there will be delay.
// if you want lower latency, you should us async rising edge (p99)
void gpio_int_rising_edge(unsigned pin) {
    if(pin>=32)
        return;
    
    dev_barrier();
    uint32_t reg = GET32(GPREN0);

    // enable rising edge detection for the given pin
    PUT32(GPREN0, reg | (1 << pin));
    dev_barrier();

    // enable the interrupt for the given pin
    PUT32(IRQ_Enable_2, (1 << GPIO_INT0_IRQ));
    dev_barrier();
}

// p98: detect falling edge (1->0).  sampled using the system clock.  
// similarly to rising edge detection, it suppresses noise by looking for
// "100" --- i.e., is triggered after two readings of "0" and so the 
// interrupt is delayed two clock cycles.   if you want  lower latency,
// you should use async falling edge. (p99)
void gpio_int_falling_edge(unsigned pin) {
    if(pin>=32)
        return;
    
    dev_barrier();
    uint32_t reg = GET32(GPFEN0);

    // enable falling edge detection for the given pin
    PUT32(GPFEN0, reg | (1 << pin));
    dev_barrier();

    // enable the interrupt for the given pin
    PUT32(IRQ_Enable_2, (1 << GPIO_INT0_IRQ));
    dev_barrier();
}

// p96: a 1<<pin is set in EVENT_DETECT if <pin> triggered an interrupt.
// if you configure multiple events to lead to interrupts, you will have to 
// read the pin to determine which caused it.
int gpio_event_detected(unsigned pin) {
    if(pin>=32)
        return 0;

    dev_barrier();
    uint32_t reg = GET32(GPEDS0);
    dev_barrier();
    return reg & (1 << pin);
}

// p96: have to write a 1 to the pin to clear the event.
void gpio_event_clear(unsigned pin) {
    if(pin>=32)
        return;
    dev_barrier();
    // clear the event by writing a 1 to the pin
    PUT32(GPEDS0, (1 << pin));
    dev_barrier();
}
