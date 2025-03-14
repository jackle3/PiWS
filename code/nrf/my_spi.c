// Bit bang implementation of SPI

enum {
    mosi        = 10,
    miso        = 9,
    clk         = 11,
    chip_0_ce   = 8,
    chip_1_ce   = 7,
};


#include "rpi.h"
#include "spi.h"
#include "cycle-util.h"
#include "cycle-count.h"

void spi_sw_init(unsigned chip_select, unsigned clock_divider) {
    dev_barrier();
    gpio_set_output(mosi); // Configure the SPI pins
    gpio_set_input(miso);
    gpio_set_output(clk);
    gpio_set_output(chip_select);
    cs = chip_select;
    

    // either chip_0_ce or chip_1_ce
    gpio_write(chip_select, 1); // CS set high
    gpio_write(clk, 0); // Clk set low
    // delay_cycles(2);
    dev_barrier();
}

spi_t spi_n_init(unsigned chip_select, unsigned clk_div) {
    spi_t dev;
    dev.chip = chip_select;
    dev.div = clk_div;
    dev.mosi = mosi;
    dev.miso = miso;
    dev.clk = clk;
    dev.ce = 8 - chip_select;
    spi_sw_init(dev.ce, clk_div);
    return dev;
}

int spi_sw_transfer(uint8_t rx[], const uint8_t tx[], unsigned nbytes) {
    dev_barrier();
    gpio_write(cs, 0); // Pull low for at least 1 bit cycle
    delay_cycles(2);

    // Main loop to bit bang
    uint32_t i;
    for (i = 0; i < nbytes; i++) {
        uint8_t byte_out = tx[i];
        uint8_t byte_in = 0;
        for (int bit = 7; bit >= 0; bit--) {
            gpio_write(mosi, (byte_out >> 7));
            byte_out = byte_out << 1;
            // delay_cycles(2); 

            gpio_write(clk, 1);
            byte_in = (byte_in << 1) | gpio_read(miso);
            // delay_cycles(2);
            gpio_write(clk, 0);
        }
        rx[i] = byte_in; 
    }
    gpio_write(cs, 1); // Pull high for at least 1 bit cycle
    // delay_cycles(2);
    dev_barrier();
    return 0;
}

int spi_n_transfer(spi_t s, uint8_t rx[], const uint8_t tx[], unsigned nbytes) {
    dev_barrier();
    gpio_write(s.ce, 0); // Pull low for at least 1 bit cycle
    // delay_cycles(85); // Not needed?

    // Main loop to bit bang
    uint32_t i;
    for (i = 0; i < nbytes; i++) {
        uint8_t byte_out = tx[i];
        uint8_t byte_in = 0;
        for (int bit = 7; bit >= 0; bit--) {
            gpio_write(s.mosi, (byte_out >> 7));
            byte_out = byte_out << 1;
            // delay_cycles(2); 

            gpio_write(s.clk, 1);
            byte_in = (byte_in << 1) | gpio_read(s.miso);
            // delay_cycles(2);
            gpio_write(s.clk, 0);
        }
        rx[i] = byte_in; 
    }
    gpio_write(s.ce, 1); // Pull high for at least 1 bit cycle
    // delay_cycles(2);
    dev_barrier();
    return 0; 
}