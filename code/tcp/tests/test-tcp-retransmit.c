#include <string.h>

#include "nrf-test.h"
#include "tcp.h"

static void test_tcp_reliable_delivery(nrf_t *server_nrf, nrf_t *client_nrf) {
    // Create TCP connections
    trace("Creating TCP connections...\n");
    struct tcp_connection *server = tcp_init(server_nrf, client_nrf->rxaddr, true);
    struct tcp_connection *client = tcp_init(client_nrf, server_nrf->rxaddr, false);

    // Handle handshake
    trace("Handshaking...\n");
    while (server->state != TCP_ESTABLISHED || client->state != TCP_ESTABLISHED) {
        tcp_do_handshake(server);
        tcp_do_handshake(client);
    }

    trace("Connection established!\n\n");

    // Send test data
    trace("Sending test data...\n");
    const char *test_msg =
        "This is a really long TCP message that will be sent over NRF. How are you doing today?";
    size_t msg_len = strlen(test_msg);
    size_t num_segments = (msg_len + RCP_MAX_PAYLOAD - 1) / RCP_MAX_PAYLOAD;
    trace("Message length: %d bytes, will be sent in %d RCP segments\n", msg_len, num_segments);
    size_t written = bytestream_write(client->sender->outgoing, test_msg, msg_len);

    // Counter to simulate packet loss
    int packet_counter = 0;

    // Alternate between client and server until data is received
    while (bytestream_bytes_available(server->receiver->incoming) < strlen(test_msg)) {
        // Client side: send segment if available
        sender_fill_window(client->sender);  // Fill window with new segments
        const struct unacked_segment *seg = sender_next_segment(client->sender);
        if (seg) {
            trace("Client sending segment seq=%d to NRF addr %x...\n\t%s\n", seg->seqno,
                  client->remote_addr, seg->data);
            tcp_send_segment(client, seg);
        }

        // Check for ACK
        struct rcp_datagram ack = rcp_datagram_init();
        if (tcp_recv_packet(client, &ack) == 0 && rcp_has_flag(&ack.header, RCP_FLAG_ACK)) {
            // Randomly drop some ACKs to force retransmission
            if (packet_counter % 3 != 0) {  // Drop every 3rd ACK
                trace("Client received ACK for seq=%d from RCP addr %x\n", ack.header.ackno,
                      ack.header.src);
                sender_process_ack(client->sender, &ack.header);
            } else {
                trace("[DROPPED ACK] Simulating dropped ACK for seq=%d\n", ack.header.ackno);
            }
        }

        // Server side: receive packet
        struct rcp_datagram dgram = rcp_datagram_init();
        if (tcp_recv_packet(server, &dgram) == 0) {
            // Randomly drop some packets to force retransmission
            if (packet_counter % 4 != 0) {  // Drop every 4th packet
                if (receiver_process_segment(server->receiver, &dgram) == 0) {
                    trace("Server received segment seq=%d from RCP addr %x, next_seqno=%d\n",
                          dgram.header.seqno, dgram.header.src,
                          server->receiver->reasm->next_seqno);
                    struct rcp_header ack = {0};
                    receiver_get_ack(server->receiver, &ack);
                    trace("Server sending ACK for seq=%d to RCP addr %x\n", ack.ackno, ack.src);
                    tcp_send_ack(server, &ack);
                }
            } else {
                trace("[DROPPED DATA] Simulating dropped packet seq=%d\n", dgram.header.seqno);
            }
        }

        // Client checks for retransmission; resends data packets if so
        tcp_check_retransmit(client, timer_get_usec());

        packet_counter++;
    }

    trace("Finished sending data\n\n");

    // Read received data
    char buffer[100];
    size_t read = bytestream_read(server->receiver->incoming, buffer, sizeof(buffer));
    printk("Server received: %s\n\n", buffer);
    assert(read == strlen(test_msg));
    assert(memcmp(buffer, test_msg, strlen(test_msg)) == 0);

    // Clean up
    trace("Closing connections...\n");
    tcp_close(client);
    tcp_close(server);
}

void notmain(void) {
    kmalloc_init(64);

    trace("configuring no-ack server=[%x] with %d nbyte msgs\n", server_addr, RCP_TOTAL_SIZE);
    nrf_t *s = server_mk_noack(server_addr, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable server config:\n", s);

    trace("configuring no-ack client=[%x] with %d nbyte msg\n", client_addr, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_noack(client_addr, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable client config:\n", c);

    // Check compatibility
    if (!nrf_compat(c, s))
        panic("did not configure correctly: not compatible\n");

    // Reset stats
    nrf_stat_start(s);
    nrf_stat_start(c);

    trace("Starting test...\n");

    test_tcp_reliable_delivery(s, c);

    // Print stats
    nrf_stat_print(s, "server: done with test");
    nrf_stat_print(c, "client: done with test");
}