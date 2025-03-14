#include <string.h>

#include "nrf-test.h"
#include "tcp.h"

static void test_tcp_reliable_delivery(nrf_t *server_nrf, nrf_t *client_nrf) {
    // Create TCP connections
    output("Creating TCP connections...\n");

    // Server is host, remote is client
    struct tcp_connection *server = tcp_init(server_nrf, client_addr, true);

    // Client is host, remote is server
    struct tcp_connection *client = tcp_init(client_nrf, server_addr, false);

    // Handle handshake
    output("Handshaking...\n");
    while (server->state != TCP_ESTABLISHED || client->state != TCP_ESTABLISHED) {
        tcp_do_handshake(server);
        tcp_do_handshake(client);
    }

    output("Connection established!\n\n");

    // Send test data
    output("Sending test data...\n");
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
    output("Message length: %d bytes, will be sent in %d RCP segments\n\n", msg_len, num_segments);

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
                output("Wrote %d bytes to bytestream, total written: %d\n", written,
                      bytes_written);
            }
        }

        // Client side: send segment if available
        sender_fill_window(client->sender);  // Fill window with new segments
        const struct unacked_segment *seg = sender_next_segment(client->sender);
        if (seg) {
            output("Client sending segment seq=%d to NRF addr %x...\n", seg->seqno,
                  client->remote_addr);
            tcp_send_segment(client, seg);
        }

        // Server side: receive packet
        struct rcp_datagram dgram = rcp_datagram_init();
        if (tcp_recv_packet(server, &dgram) == 0) {
            int result = receiver_process_segment(server->receiver, &dgram);
            // output("Server processed segment seq=%d with result %d\n", dgram.header.seqno,
            // result);
            if (result >= 0) {  // Process succeeded or was retransmission
                output("    Server received segment seq=%d from RCP addr %x, next_seqno=%d\n",
                      dgram.header.seqno, dgram.header.src, server->receiver->reasm->next_seqno);

                struct rcp_header ack = {0};
                receiver_get_ack(server->receiver, &ack);
                output("    Server sending ACK for seq=%d to RCP addr %x\n", ack.ackno, ack.src);
                tcp_send_ack(server, &ack);
            }
        }

        // Client side: check for ACK
        struct rcp_datagram ack = rcp_datagram_init();
        if (tcp_recv_packet(client, &ack) == 0 && rcp_has_flag(&ack.header, RCP_FLAG_ACK)) {
            output("Client received ACK for seq=%d from RCP addr %x\n", ack.header.ackno,
                  ack.header.src);
            sender_process_ack(client->sender, &ack.header);
        }

        // Read any available data from server's incoming bytestream
        size_t remaining_to_read = msg_len - bytes_read;
        size_t read =
            bytestream_read(server->receiver->incoming, buffer + bytes_read, remaining_to_read);
        bytes_read += read;
        if (read > 0) {
            output("Read %d more bytes from bytestream, total read: %d\n", read, bytes_read);
        }

        // Client checks for retransmission; resends data packets if so
        tcp_check_retransmit(client, timer_get_usec());
    }

    output("Finished sending data\n\n");

    // Verify received data
    printk("Server received:\n\n%s\n\n", buffer);
    assert(bytes_read == msg_len);
    assert(memcmp(buffer, test_msg, msg_len) == 0);

    // Clean up - proper TCP closing
    output("Starting proper TCP closing sequence...\n");
    
    // Client initiates active close
    output("Client initiating active close...\n");
    tcp_close(client);
    output("Client closed: state=%d\n", client->state);
    
    // Server should now be in CLOSE_WAIT state
    // We need to monitor the server to ensure it receives the FIN
    output("Waiting for server to receive client's FIN...\n");
    
    // Wait for server to enter CLOSE_WAIT (it received FIN)
    int max_iterations = 100;  // Limit iterations to prevent infinite loop
    int iterations = 0;
    while (server->state != TCP_CLOSE_WAIT && iterations < max_iterations) {
        iterations++;
        
        // Try to receive the FIN packet
        struct rcp_datagram fin_dgram = rcp_datagram_init();
        if (tcp_recv_packet(server, &fin_dgram) == 0) {
            // If server receives a FIN, it will go to CLOSE_WAIT 
            if (rcp_has_flag(&fin_dgram.header, RCP_FLAG_FIN) && 
                server->state == TCP_ESTABLISHED) {
                output("Server received FIN, transitioning to CLOSE_WAIT\n");
                
                // Send ACK for the FIN
                struct rcp_header ack = {0};
                ack.src = server->sender->src_addr;
                ack.dst = server->sender->dst_addr;
                ack.seqno = server->sender->next_seqno;
                ack.ackno = fin_dgram.header.seqno + 1;
                rcp_set_flag(&ack, RCP_FLAG_ACK);
                rcp_compute_checksum(&ack);
                tcp_send_ack(server, &ack);
                
                // Transition to CLOSE_WAIT
                server->state = TCP_CLOSE_WAIT;
                break;
            }
        }
        
        delay_ms(1);  // Short delay
    }
    
    // Server completes passive close from CLOSE_WAIT
    if (server->state == TCP_CLOSE_WAIT) {
        output("Server in CLOSE_WAIT, completing passive close...\n");
        tcp_close(server);
        output("Server closed: state=%d\n", server->state);
    } else {
        output("WARNING: Server did not receive FIN after %d iterations\n", max_iterations);
    }
    
    // Verify both sides closed properly
    assert(client->state == TCP_CLOSED);
    assert(server->state == TCP_CLOSED);
    output("Both connections successfully closed!\n");
}

void notmain(void) {
    kmalloc_init(64);

    output("configuring no-ack server=[%x] with %d nbyte msgs\n", server_addr_2, RCP_TOTAL_SIZE);
    nrf_t *s = server_mk_noack(server_addr_2, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable server config:\n", s);

    output("configuring no-ack client=[%x] with %d nbyte msg\n", client_addr_2, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_noack(client_addr_2, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable client config:\n", c);

    // Check compatibility
    if (!nrf_compat(c, s))
        panic("did not configure correctly: not compatible\n");

    // Reset stats
    nrf_stat_start(s);
    nrf_stat_start(c);

    output("Starting test...\n");

    test_tcp_reliable_delivery(s, c);

    // Print stats
    nrf_stat_print(s, "server: done with test");
    nrf_stat_print(c, "client: done with test");
}