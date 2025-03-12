#include "rpi.h"
#include "nrf-test.h"
#include "circular.h"

#define PAYLOAD_SIZE 32

// Shared circular queue for both sending and receiving
static cq_t shared_queue;

// Main looping function that handles both sending and receiving
static void nrf_loop(nrf_t *tx_nrf, nrf_t *rx_nrf)
{
    uint8_t payload[PAYLOAD_SIZE];
    unsigned dst_addr = client_addr; // Address to send to

    while (1)
    {
        // Try to send if we have data in the queue
        if (cq_pop_n_noblk(&shared_queue, payload, PAYLOAD_SIZE))
        {
            if (nrf_send_ack(tx_nrf, dst_addr, payload, PAYLOAD_SIZE))
            {
                // print entire payload as string
                trace("sent packet %s\n", payload);
            }
        }

        // Check for received data
        if (nrf_read_exact_timeout(rx_nrf, payload, PAYLOAD_SIZE, 100) == PAYLOAD_SIZE)
        {
            trace("received packet %s\n", payload);
            // Push received data back to queue to keep it looping
            while (!cq_push_n(&shared_queue, payload, PAYLOAD_SIZE))
            {
                delay_us(100); // Wait if queue is full
            }
        }

        // Small delay to prevent tight spinning
        delay_us(100);
    }
}

void notmain(void)
{
    kmalloc_init(1024); // Initialize with larger heap for safety

    // Initialize the circular queue
    cq_init(&shared_queue, 1); // errors are fatal

    // Setup NRF modules
    nrf_t *server = server_mk_ack(server_addr, PAYLOAD_SIZE);
    nrf_t *client = client_mk_ack(client_addr, PAYLOAD_SIZE);

    // Create initial payload
    uint8_t initial_payload[PAYLOAD_SIZE];
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        initial_payload[i] = 'a' + i; // Fill with incrementing values
    }

    // Push initial payload to start the loop
    while (!cq_push_n(&shared_queue, initial_payload, PAYLOAD_SIZE))
    {
        delay_us(100);
    }

    // Enable statistics for monitoring
    nrf_stat_start(server);
    nrf_stat_start(client);

    trace("starting nrf loop\n");

    // Start the main loop
    nrf_loop(server, client);
}
