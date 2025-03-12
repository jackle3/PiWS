#include "rpi.h"
#include "nrf-test.h"
#include "circular.h"
#include "rcp-datagram.h"
#include "uart-to-tcp.h"

#define PAYLOAD_SIZE 32

// Shared circular queue for both sending and receiving
static cq_t tx_queue; // Queue for outgoing messages
static cq_t rx_queue; // Queue for received messages

// Main looping function that handles both sending and receiving
static void nrf_loop(nrf_t *tx_nrf, nrf_t *rx_nrf)
{
    uint8_t payload[PAYLOAD_SIZE];
    unsigned dst_addr = client_addr; // Address to send to
    struct rcp_datagram segment = {head, NULL};
    int i = 0;
    uint8_t *data = kmalloc(22);
    sw_uart_putk(&u, "Enter message, max 22 chars: \n");
    while (1)
    {
        // Poll user for input

        // Create the packet
        // Write data
        char c = sw_uart_get8(&u); // oops this is blocking so this is shit.
        data[i++] = c;

        // trace("%c\n", c);

        if (i == 21 || c == '\n')
        {
            data[i] = '\0';
            // sw_uart_putk(&u, "exit loop");
            // sw_uart_putk(&u, data);
            // Kmalloc for the payload, then return
            segment.header.payload_len = i + 1;
            segment.payload = data;
            // sw_uart_putk(&u, "done");
            rcp_compute_checksum(&segment.header);
            // sw_uart_putk(&u, "done");
            return segment;
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
            struct rcp_datagram segment = {head, NULL};
            int i = 0;
            uint8_t *data = kmalloc(22);
            sw_uart_putk(&u, "Enter message, max 22 chars: \n");
        }

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
