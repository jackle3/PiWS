#include "rpi.h"
#include "eqx-threads.h"
#include "nrf-test.h"
#include "circular.h"
#include "rcp-datagram.h"
#include "uart-to-tcp.h"

#define PAYLOAD_SIZE 32

// Shared circular queues
static cq_t tx_queue; // Queue for outgoing messages
static cq_t rx_queue; // Queue for received messages

// Thread function for handling user input
void input_thread(void *arg)
{
    // output("Starting UART-to-TCP test...\n");
    // // hw_uart_disable();
    // sw_uart_t u = sw_uart_init(14, 15, 115200);
    // output("UART initialized\n");
    // sw_uart_putk(&u, "Hello from NRF!\n");
    // config_init(u);
    sw_uart_t *u = (sw_uart_t *)arg;
    while (1)
    {
        // Get user input and create payload
        struct rcp_datagram dgram = create_packet(*u);
        uint8_t *dgram_serialized = kmalloc(PAYLOAD_SIZE);
        rcp_datagram_serialize(&dgram, dgram_serialized, PAYLOAD_SIZE);

        // Push to transmit queue
        while (!cq_push_n(&tx_queue, dgram_serialized, PAYLOAD_SIZE))
        {
            delay_us(100); // Wait if queue is full
        }

        sw_uart_putk(u, "Message queued for transmission\n");
        delay_ms(1000); // Delay before next prompt
    }
}

// Thread function for NRF communication
void nrf_thread(void *arg)
{
    nrf_t *tx_nrf = ((nrf_t **)arg)[0];
    nrf_t *rx_nrf = ((nrf_t **)arg)[1];
    uint8_t payload[PAYLOAD_SIZE];
    unsigned dst_addr = client_addr;

    while (1)
    {
        // Try to send if we have data in the tx queue
        if (cq_pop_n_noblk(&tx_queue, payload, PAYLOAD_SIZE))
        {
            if (nrf_send_ack(tx_nrf, dst_addr, payload, PAYLOAD_SIZE))
            {
                trace("sent message: %s\n", payload);
            }
        }

        // Check for received data
        if (nrf_read_exact_timeout(rx_nrf, payload, PAYLOAD_SIZE, 100) == PAYLOAD_SIZE)
        {
            trace("received message: %s\n", payload);
            // Store in receive queue
            while (!cq_push_n(&rx_queue, payload, PAYLOAD_SIZE))
            {
                delay_us(100); // Wait if queue is full
            }
        }

        delay_us(100); // Small delay to prevent tight spinning
    }
}

void notmain(void)
{
    kmalloc_init(1024);
    hw_uart_disable();
    sw_uart_t u = sw_uart_init(14, 15, 115200);
    config_init(u);
    sw_uart_putk(&u, "UART initialized\n");

    // Initialize the circular queues
    cq_init(&tx_queue, 1);
    cq_init(&rx_queue, 1);
    sw_uart_putk(&u, "Circular queues initialized\n");

    // Setup NRF modules
    nrf_t *server = server_mk_ack(server_addr, PAYLOAD_SIZE);
    nrf_t *client = client_mk_ack(client_addr, PAYLOAD_SIZE);

    sw_uart_putk(&u, "NRF modules initialized\n");

    // Package NRF pointers for thread
    nrf_t *nrf_modules[2] = {server, client};

    // Enable statistics for monitoring
    nrf_stat_start(server);
    nrf_stat_start(client);

    sw_uart_putk(&u, "starting threads\n");

    eqx_init();
    // Start threads
    eqx_th_t *th1 = eqx_fork(input_thread, &u, 0);
    eqx_th_t *th2 = eqx_fork(nrf_thread, nrf_modules, 0);
    // input_thread(&u);

    // Run threads
    eqx_run_threads();
}
