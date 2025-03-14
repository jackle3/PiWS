#include <string.h>

#include "nrf-test.h"
#include "tcp.h"
#include "uart-to-tcp.h"

#define BUFFER_SIZE 256

static void test_tcp_reliable_delivery(nrf_t *server_nrf, nrf_t *client_nrf)
{
    // Create TCP connections
    // trace("Creating TCP connections...\n");

    // Server is host, remote is client
    struct tcp_connection *server = tcp_init(server_nrf, client_addr, true);

    // Client is host, remote is server
    struct tcp_connection *client = tcp_init(client_nrf, server_addr, false);
    size_t other_addr = nrf_to_rcp_addr(client_addr);
    char *other_addr_str = rcp_to_string(other_addr);
    size_t my_addr = nrf_to_rcp_addr(server_nrf->rxaddr);
    char *my_addr_str = rcp_to_string(my_addr);

    // Handle handshake
    trace("Handshaking...\n");
    while (server->state != TCP_ESTABLISHED || client->state != TCP_ESTABLISHED)
    {
        tcp_server_handshake(server);
        tcp_client_handshake(client);
    }

    trace("Connection established to %s!\n\n", other_addr_str);

    uart_putk("Enter messages below, type 'quit' to exit...\n");

    // Send test data
    // trace("Sending test data...\n");
    // size_t msg_len = strlen(msg_buffer);
    // size_t num_segments = (msg_len + RCP_MAX_PAYLOAD - 1) / RCP_MAX_PAYLOAD;
    // trace("Message length: %d bytes, will be sent in %d RCP segments\n", msg_len, num_segments);

    // Track how much of the message has been written and read
    size_t bytes_written = 0;
    size_t bytes_read = 0;
    char buffer[BUFFER_SIZE];
    char received_buffer[BUFFER_SIZE];
    size_t buffer_current_length = 0;

    while (1)
    {
        if (uart_has_data())
        {
            while (1)
            {
                char c = uart_get8();
                // if newline, check if there's more lines afterwards, otherwise break
                if (c == '\n')
                {
                    delay_us(100);
                    if (!uart_has_data())
                    {
                        break;
                    }
                }
                buffer[buffer_current_length++] = c;
                if (buffer_current_length >= BUFFER_SIZE)
                {
                    break;
                }
            }
        }
        // if message is "quit", break
        if (buffer_current_length == 4 && strncmp(buffer, "quit", 4) == 0)
        {
            break;
        }
        // Write more data if there's space in the bytestream
        size_t remaining = buffer_current_length;
        if (remaining > 0)
        {
            size_t written =
                bytestream_write(client->sender->outgoing, buffer, remaining);
            bytes_written += written;
            buffer_current_length = 0;
            // if (written > 0)
            // {
            //     trace("Wrote %d more bytes to bytestream, total written: %d\n", written,
            //           bytes_written);
            // }
        }

        // Client side: send segment if available
        sender_fill_window(client->sender); // Fill window with new segments
        const struct unacked_segment *seg = sender_next_segment(client->sender);
        if (seg)
        {

            // trace("Client sending segment seq=%d to NRF addr %x...\n\t%s\n", seg->seqno,
            //       client->remote_addr, seg->data);
            tcp_send_segment(client, seg);
        }

        // Server side: receive packet
        struct rcp_datagram dgram = rcp_datagram_init();
        if (tcp_recv_packet(server, &dgram) == 0)
        {
            int result = receiver_process_segment(server->receiver, &dgram);
            // trace("Server processed segment seq=%d with result %d\n", dgram.header.seqno, result);
            if (result >= 0)
            { // Process succeeded or was retransmission
                // trace("Server received segment seq=%d from RCP addr %x, next_seqno=%d\n",
                //       dgram.header.seqno, dgram.header.src, server->receiver->reasm->next_seqno);

                struct rcp_header ack = {0};
                receiver_get_ack(server->receiver, &ack);
                // trace("Server sending ACK for seq=%d to RCP addr %x\n", ack.ackno, ack.src);
                tcp_send_ack(server, &ack);
            }
        }

        // Client side: check for ACK
        struct rcp_datagram ack = rcp_datagram_init();
        if (tcp_recv_packet(client, &ack) == 0 && rcp_has_flag(&ack.header, RCP_FLAG_ACK))
        {
            // trace("Client received ACK for seq=%d from RCP addr %x\n", ack.header.ackno,
            //       ack.header.src);
            sender_process_ack(client->sender, &ack.header);
        }

        // Read all available data from server's incoming bytestream
        size_t read =
            bytestream_read(server->receiver->incoming, received_buffer, RECEIVER_BUFFER_SIZE);
        bytes_read += read;
        if (read > 0)
        {
            received_buffer[read] = '\0';
            uart_putk("From ");
            uart_putk(other_addr_str);
            uart_putk(": ");
            uart_putk(received_buffer);
            uart_putk("\n");
        }
        // if (read > 0)
        // {
        //     trace("Read %d more bytes from bytestream, total read: %d\n", read, bytes_read);
        // }

        // Client checks for retransmission; resends data packets if so
        tcp_check_retransmit(client, timer_get_usec());
    }

    // trace("Finished sending data\n\n");

    // Verify received data
    // printk("Server received: %s\n\n", received_buffer);
    // assert(bytes_read == msg_len);
    // assert(memcmp(buffer, test_msg, msg_len) == 0);

    // Clean up
    trace("Closing connections...\n");
    tcp_close(client);
    tcp_close(server);
}

void notmain(void)
{
    kmalloc_init(64);
    uart_init();
    config_init_hw(); // TODO: actually have destination address do something

    trace("configuring no-ack server=[%x] with %d nbyte msgs\n", server_addr_2, RCP_TOTAL_SIZE);
    nrf_t *s = server_mk_noack(server_addr_2, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable server config:\n", s);

    trace("configuring no-ack client=[%x] with %d nbyte msg\n", client_addr_2, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_noack(client_addr_2, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable client config:\n", c);

    // Check compatibility
    if (!nrf_compat(c, s))
        panic("did not configure correctly: not compatible\n");

    // Reset stats
    nrf_stat_start(s);
    nrf_stat_start(c);

    // trace("Starting test...\n");

    test_tcp_reliable_delivery(s, c);

    // Print stats
    nrf_stat_print(s, "server: done with test");
    nrf_stat_print(c, "client: done with test");
}