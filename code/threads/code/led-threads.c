#include "rpi.h"
#include "eqx-threads.h"

// Pin numbers for LEDs
#define LED1 20
#define LED2 21

int num_blinks = 5;

// Thread function for slow blinking LED
void slow_blink(void *arg)
{
    // gpio_set_output(LED1);
    for (int i = 0; i < num_blinks; i++)
    {
        gpio_set_on(LED1);
        delay_ms(100);
        gpio_set_off(LED1);
        delay_ms(100);
    }
}

// Thread function for fast blinking LED
void fast_blink(void *arg)
{
    // gpio_set_output(LED2);
    for (int i = 0; i < num_blinks * 5; i++)
    {
        gpio_set_on(LED2);
        delay_ms(20);
        gpio_set_off(LED2);
        delay_ms(20);
    }
}

void notmain(void)
{
    eqx_verbose(0);
    eqx_init();
    gpio_set_output(LED1);
    gpio_set_output(LED2);

    // Create and run threads individually first
    eqx_th_t *th1 = eqx_fork(fast_blink, 0, 0xf82f1634);
    // th1->verbose_p = 1;
    // eqx_run_threads();

    eqx_th_t *th2 = eqx_fork(slow_blink, 0, 0);
    // th2->verbose_p = 1;

    eqx_run_threads();

    // output("---------------------------------------------------\n");
    // output("about to do quiet run\n");
    // eqx_verbose(0);

    // Refork and run both threads together
    // eqx_refork(th1);
    // eqx_refork(th2);
    // eqx_run_threads();
}
