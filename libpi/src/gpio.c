#include "rpi.h"

enum {
    GP_OFFSET = 0x4,
    GP_BASE = 0x20200000,
    GP_SET0 = (GP_BASE + 0x1C),
    GP_SET1 = (GP_BASE + 0x20),
    GP_CLR0 = (GP_BASE + 0x28),
    GP_CLR1 = (GP_BASE + 0x2C),
    GP_LEV0 = (GP_BASE + 0x34),
    GP_LEV1 = (GP_BASE + 0x38),
    GP_PUD = (GP_BASE + 0x94),
    GP_PUDCLK0 = (GP_BASE + 0x98),
    GP_PUDCLK1 = (GP_BASE + 0x9C),
};

/**
 * set GPIO function for <pin> (input, output, alt...).
 * settings for other pins should be unchanged.
 */
void gpio_set_function(unsigned pin, gpio_func_t function) {
    if (pin >= 32 && pin != 47)
        return;

    if (function < 0 || function > 7)
        return;

    // Calculate the FSELn register to edit
    unsigned fsel_offset = (pin / 10) * GP_OFFSET;
    unsigned shift_amount = (pin % 10) * 3;

    unsigned fsel_register = GP_BASE + fsel_offset;
    unsigned mask = 0b111 << shift_amount;

    unsigned value = GET32(fsel_register);
    value &= ~mask;  // zero out existing bits for this pin
    value |= function << shift_amount;
    PUT32(fsel_register, value);
}

/**
 * Configure GPIO <pin> as an output pin.
 */
void gpio_set_output(unsigned pin) { gpio_set_function(pin, GPIO_FUNC_OUTPUT); }

/**
 * Set GPIO <pin> on.
 */
void gpio_set_on(unsigned pin) {
    if (pin >= 32 && pin != 47)
        return;

    unsigned set_register = (pin < 32) ? GP_SET0 : GP_SET1;
    PUT32(set_register, (1 << (pin % 32)));
}

/**
 * Set GPIO<pin> off.
 */
void gpio_set_off(unsigned pin) {
    if (pin >= 32 && pin != 47)
        return;

    unsigned clr_register = (pin < 32) ? GP_CLR0 : GP_CLR1;
    PUT32(clr_register, (1 << (pin % 32)));
}

/**
 * Configure GPIO <pin> as an input pin.
 */
void gpio_set_input(unsigned pin) { gpio_set_function(pin, GPIO_FUNC_INPUT); }

/**
 * Set GPIO <pin> to <v>.
 *
 * <v> should be interepreted as a C "boolean"; i.e., 0 is
 * false, anything else is true.
 */
void gpio_write(unsigned pin, unsigned v) {
    if (v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

/**
 * Read the current value of GPIO <pin>.
 * Returns either 0 or 1.
 */
int gpio_read(unsigned pin) {
    if (pin >= 32)
        return -1;

    // bit i of levels contains 0 or 1, indicating whether GPIO i is on or not
    unsigned levels = GET32(GP_LEV0);
    unsigned pin_value = (levels >> (pin % 32)) & 1;
    return DEV_VAL32(pin_value);
}

/**
 * Set GPIO <pin> to pullup or pulldown or off based on <state>.
 */
void gpio_set_pull(unsigned pin, unsigned state) {
    dev_barrier();

    if (pin >= 32)
        return;

    // Select pullup (2), pulldown (1), or off (0) based on state
    PUT32(GP_PUD, state);

    // Wait 150 cycles for setup
    dev_barrier();
    delay_cycles(150);
    dev_barrier();

    // Clock the control signal into the pin
    unsigned pud_register = (pin < 32) ? GP_PUDCLK0 : GP_PUDCLK1;
    PUT32(pud_register, 1 << (pin % 32));

    // Wait 150 cycles for the clock to be recognized
    dev_barrier();
    delay_cycles(150);
    dev_barrier();

    // Remove the control signal and clock
    PUT32(GP_PUD, 0);
    PUT32(pud_register, 0);

    dev_barrier();
}

/**
 * Activate the pullup register on GPIO <pin>.
 *
 * GPIO <pin> must be an input pin.
 *
 * A pullup makes the pin default to reading '1' when nothing is connected.
 * This prevents the pin from giving random readings when left floating.
 */
void gpio_set_pullup(unsigned pin) { gpio_set_pull(pin, 0b10); }

/**
 * Activate the pulldown register on GPIO <pin>.
 *
 * GPIO <pin> must be an input pin.
 *
 * A pulldown makes the pin default to reading '0' when nothing is connected.
 * This prevents the pin from giving random readings when left floating.
 */
void gpio_set_pulldown(unsigned pin) { gpio_set_pull(pin, 0b01); }

/**
 * Deactivate both the pullup and pulldown registers on GPIO <pin>.
 *
 * GPIO <pin> must be an input pin.
 */
void gpio_pud_off(unsigned pin) { gpio_set_pull(pin, 0b00); }