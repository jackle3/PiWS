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
    uint8_t dgram_serialized[PAYLOAD_SIZE];
    unsigned dst_addr = client_addr_2; // Address to send to
    struct rcp_datagram segment = {head, NULL};
    int i = 0;
    uint8_t *data = kmalloc(220);
    uart_putk("Enter message, max 220 chars: \n");
    while (1)
    {
        // Poll user for input

        // Create the packet
        // Write data
        if (uart_has_data())
        {
            char c = uart_get8();
            data[i++] = c;

            // trace("%c\n", c);

            if (i == 219 || c == '\n')
            {
                data[i] = '\0';
                // Serialize packets:
                int num_packets = (i - 1) / 22;
                int remaining = (i - 1) % 22;
                for (int j = 0; j < num_packets; j++)
                {
                    rcp_datagram_set_payload(&segment, &data[j * 22], 22);
                    rcp_compute_checksum(&segment.header);
                    // TODO replace printout with enqueue
                    for (int x = 0; x < segment.header.payload_len; x++)
                    {
                        uart_put8(segment.payload[x]);
                    }
                    uart_putk("\n");
                }
                if (remaining)
                {
                    rcp_datagram_set_payload(&segment, &data[num_packets * 22], remaining);
                    rcp_compute_checksum(&segment.header);
                    // todo enqueue, also printed out by test
                }
                uint8_t *dgram_serialized = kmalloc(PAYLOAD_SIZE);
                rcp_datagram_serialize(&segment, dgram_serialized, PAYLOAD_SIZE);

                // Push to transmit queue
                while (!cq_push_n(&tx_queue, dgram_serialized, PAYLOAD_SIZE))
                {
                    delay_us(100); // Wait if queue is full
                }

                uart_putk("Message queued for transmission\n");
                delay_ms(1000); // Delay before next prompt
                segment = rcp_datagram_init();
                i = 0;
                uart_putk("Enter message, max 220 chars: \n");
            }
        }

        // Try to send if we have data in the queue
        if (cq_pop_n_noblk(&tx_queue, dgram_serialized, PAYLOAD_SIZE))
        {
            if (nrf_send_ack(tx_nrf, dst_addr, dgram_serialized, PAYLOAD_SIZE))
            {
                // print payload from rcp datagram
                struct rcp_datagram temp = rcp_datagram_init();
                rcp_datagram_parse(&temp, dgram_serialized, PAYLOAD_SIZE);
                trace("sent packet %s\n", temp.payload);
            }
        }

        // Check for received data
        if (nrf_read_exact_timeout(rx_nrf, dgram_serialized, PAYLOAD_SIZE, 100) == PAYLOAD_SIZE)
        {
            struct rcp_datagram temp = rcp_datagram_init();
            rcp_datagram_parse(&temp, dgram_serialized, PAYLOAD_SIZE);
            trace("received packet %s\n", temp.payload);
            // Push received data back to queue to keep it looping
            while (!cq_push_n(&rx_queue, dgram_serialized, PAYLOAD_SIZE))
            {
                delay_us(100); // Wait if queue is full
            }
            trace("pushed to rx queue\n");
        }

        // Small delay to prevent tight spinning
        delay_us(100);
    }
}

void notmain(void)
{
    kmalloc_init(1024); // Initialize with larger heap for safety
    uart_init();

    // Initialize the circular queue
    cq_init(&tx_queue, 1); // errors are fatal
    cq_init(&rx_queue, 1); // errors are fatal

    // Setup NRF modules
    nrf_t *server = server_mk_ack(server_addr, PAYLOAD_SIZE);
    nrf_t *client = client_mk_ack(client_addr, PAYLOAD_SIZE);

    // Enable statistics for monitoring
    nrf_stat_start(server);
    nrf_stat_start(client);

    config_init_hw();

    trace("starting nrf loop\n");

    // Start the main loop
    nrf_loop(server, client);
}
