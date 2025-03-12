#include <string.h>

#include "nrf-test.h"
#include "tcp.h"

static void test_tcp_reliable_delivery(nrf_t *server_nrf, nrf_t *client_nrf) {
    // Create TCP connections
    trace("Creating TCP connections...\n");

    // Server is host, remote is client
    struct tcp_connection *server = tcp_init(server_nrf, client_nrf->rxaddr, true);

    // Client is host, remote is server
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
        "140e is a lab-based class with no explicit lectures. We will do two five to eight hour "
        "labs each week. You should be able to complete almost all of the lab in one sitting. "
        "There will be several homeworks, that act as mini-capstone projects tying together the "
        "preceding labs.\n\n"
        "By the end of the class you will have built your own simple, clean OS for the "
        "widely-used, ARM-based raspberry pi --- including interrupts, threads, virtual memory, "
        "and a simple file system. Your OS should serve as a good base for interesting, real, "
        "sensor-based / embedded projects.\n\n"
        "We try to work directly with primary-sources (the Broadcom and ARM6 manuals, various "
        "datasheets) since learning to understand such prose is one of the super-powers of good "
        "systems hackers. It will also give you the tools to go off on your own after the course "
        "and fearlessly build sensor-based devices using only their datasheets.\n\n"
        "This course differs from most OS courses in that it uses real hardware instead of a fake "
        "simulator, and almost all of the code will be written by you.\n\n"
        "After this quarter, you'll know/enact many cool things your peers do not. You will also "
        "have a too-rare concrete understanding of how computation works on real hardware. This "
        "understanding will serve you in many other contexts. For what it is worth, everything you "
        "build will be stuff we found personally useful. There will be zero (intentional) "
        "busy-work.";
    size_t msg_len = strlen(test_msg);
    size_t num_segments = (msg_len + RCP_MAX_PAYLOAD - 1) / RCP_MAX_PAYLOAD;
    trace("Message length: %d bytes, will be sent in %d RCP segments\n", msg_len, num_segments);

    // Track how much of the message has been written and read
    size_t bytes_written = 0;
    size_t bytes_read = 0;
    char buffer[msg_len];

    // Keep going until all data is sent and received
    while (bytes_read < msg_len) {
        // Write more data if there's space in the bytestream
        size_t remaining = msg_len - bytes_written;
        if (remaining > 0) {
            size_t written =
                bytestream_write(client->sender->outgoing, test_msg + bytes_written, remaining);
            bytes_written += written;
            if (written > 0) {
                trace("Wrote %d more bytes to bytestream, total written: %d\n", written,
                      bytes_written);
            }
        }

        // Client side: send segment if available
        sender_fill_window(client->sender);  // Fill window with new segments
        const struct unacked_segment *seg = sender_next_segment(client->sender);
        if (seg) {
            // Drop packets based on sequence number and timing
            bool should_drop = (seg->seqno * timer_get_usec()) % 7 == 0;  // Pseudo-random dropping
            if (!should_drop) {
                trace("Client sending segment seq=%d to NRF addr %x...\n\t%s\n", seg->seqno,
                      client->remote_addr, seg->data);
                tcp_send_segment(client, seg);
            } else {
                trace("[DROPPED SEGMENT] Simulating dropped segment seq=%d\n", seg->seqno);
                // Mark as sent even though we dropped it - simulates network loss
                sender_segment_sent(client->sender, seg, timer_get_usec());
            }
        }

        // Server side: receive packet
        struct rcp_datagram dgram = rcp_datagram_init();
        if (tcp_recv_packet(server, &dgram) == 0) {
            int result = receiver_process_segment(server->receiver, &dgram);
            trace("Server processed segment seq=%d with result %d\n", dgram.header.seqno, result);
            if (result >= 0) {  // Process succeeded or was retransmission
                trace("Server received segment seq=%d from RCP addr %x, next_seqno=%d\n",
                      dgram.header.seqno, dgram.header.src, server->receiver->reasm->next_seqno);

                // Drop ACKs based on sequence number and timing
                bool should_drop_ack = ((dgram.header.seqno * timer_get_usec()) % 5) == 0;
                if (!should_drop_ack) {
                    struct rcp_header ack = {0};
                    receiver_get_ack(server->receiver, &ack);
                    trace("Server sending ACK for seq=%d to RCP addr %x\n", ack.ackno, ack.src);
                    tcp_send_ack(server, &ack);
                } else {
                    trace("[DROPPED ACK] Server dropping ACK for seq=%d\n",
                          server->receiver->reasm->next_seqno - 1);
                }
            }
        }

        // Client side: check for ACK
        struct rcp_datagram ack = rcp_datagram_init();
        if (tcp_recv_packet(client, &ack) == 0 && rcp_has_flag(&ack.header, RCP_FLAG_ACK)) {
            trace("Client received ACK for seq=%d from RCP addr %x\n", ack.header.ackno,
                  ack.header.src);
            sender_process_ack(client->sender, &ack.header);
        }

        // Read any available data from server's incoming bytestream
        size_t remaining_to_read = msg_len - bytes_read;
        size_t read =
            bytestream_read(server->receiver->incoming, buffer + bytes_read, remaining_to_read);
        bytes_read += read;
        if (read > 0) {
            trace("Read %d more bytes from bytestream, total read: %d\n", read, bytes_read);
        }

        // Client checks for retransmission; resends data packets if so
        tcp_check_retransmit(client, timer_get_usec());
    }

    trace("Finished sending data\n\n");

    // Verify received data
    printk("Server received: %s\n\n", buffer);
    assert(bytes_read == msg_len);
    assert(memcmp(buffer, test_msg, msg_len) == 0);

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