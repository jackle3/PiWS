#include "rpi.h"
#include "eqx-threads.h"
#include "nrf-test.h"
#include "circular.h"

#define PAYLOAD_SIZE 32

// Shared circular queues
static cq_t tx_queue; // Queue for data to be transmitted
static cq_t rx_queue; // Queue for received data

// Thread function for NRF sender
void sender_thread(void *arg)
{
    nrf_t *nrf = (nrf_t *)arg;
    uint8_t payload[PAYLOAD_SIZE];

    while (1)
    {
        if (cq_pop_n_noblk(&tx_queue, payload, PAYLOAD_SIZE))
        {
            nrf_send_ack(nrf, client_addr, payload, PAYLOAD_SIZE);
            nrf_stat_print(nrf, "Sent payload");
        }
        delay_us(100);
    }
}

// Thread function for NRF receiver
void receiver_thread(void *arg)
{
    nrf_t *nrf = (nrf_t *)arg;
    uint8_t payload[PAYLOAD_SIZE];

    while (1)
    {
        // Try to receive data
        if (nrf_read_exact_timeout(nrf, payload, PAYLOAD_SIZE, 1000) == PAYLOAD_SIZE)
        {
            nrf_stat_print(nrf, "Received payload");
            // Push received data to both queues to keep the loop going
            while (!cq_push_n(&tx_queue, payload, PAYLOAD_SIZE))
                delay_us(100); // Wait if queue is full
            while (!cq_push_n(&rx_queue, payload, PAYLOAD_SIZE))
                delay_us(100); // Wait if queue is full
        }
    }
}

void notmain(void)
{
    kmalloc_init(1);
    eqx_init();

    // Initialize the circular queues
    cq_init(&tx_queue, 1);
    cq_init(&rx_queue, 1);

    // Setup NRF modules
    nrf_t *server = server_mk_ack(server_addr, PAYLOAD_SIZE);
    nrf_t *client = client_mk_ack(client_addr, PAYLOAD_SIZE);

    // Create initial payload
    uint8_t initial_payload[PAYLOAD_SIZE];
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        initial_payload[i] = i;
    }

    // Push initial payload
    while (!cq_push_n(&tx_queue, initial_payload, PAYLOAD_SIZE))
        delay_us(100);

    // Start threads
    eqx_th_t *sender = eqx_fork(sender_thread, server, 0);
    // eqx_th_t *receiver = eqx_fork(receiver_thread, client, 0);

    // Enable statistics for monitoring
    nrf_stat_start(server);
    eqx_run_threads();
    nrf_stat_start(client);

    // Run threads
    eqx_run_threads();
}
