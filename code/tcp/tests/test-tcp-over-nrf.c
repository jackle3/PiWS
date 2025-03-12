#include <string.h>

#include "nrf-test.h"
#include "nrf.h"
#include "rcp-datagram.h"
#include "receiver.h"
#include "sender.h"

enum { timeout_usec = 1000 };

static void test_tcp_single_packet(nrf_t *server, nrf_t *client, int verbose_p) {
    unsigned client_addr = client->rxaddr;
    struct sender *s = sender_init(server->rxaddr, client_addr, 1000);
    struct receiver *r = receiver_init(client_addr, server->rxaddr);
    assert(s && r);

    // Create a small test message that fits in one packet
    const char *test_msg = "Hello!";
    size_t msg_len = strlen(test_msg);

    // Write to sender's stream
    size_t written = bytestream_write(s->outgoing, (uint8_t *)test_msg, msg_len);
    assert(written == msg_len);
    if (verbose_p)
        trace("wrote %d bytes to sender\n", written);

    // Fill sender's window (should create one segment)
    int segments = sender_fill_window(s);
    assert(segments == 1);

    uint32_t curr_time = 0;
    uint8_t rx_buffer[RCP_TOTAL_SIZE];
    uint8_t assembled[100] = {0};
    size_t assembled_len = 0;

    // Get the segment to send
    const struct unacked_segment *seg = sender_next_segment(s);
    assert(seg);

    // Create and send RCP packet
    struct rcp_datagram dgram = rcp_datagram_init();
    dgram.header.src = s->src_addr;
    dgram.header.dst = s->dst_addr;
    dgram.header.seqno = seg->seqno;
    dgram.header.window = s->window_size;

    if (rcp_datagram_set_payload(&dgram, seg->data, seg->len) < 0)
        panic("[SEND] payload set failed\n");

    trace("Created dgram with seqno %d and payload len %d\n", dgram.header.seqno,
          dgram.header.payload_len);

    rcp_compute_checksum(&dgram.header);

    // Serialize and send packet
    int packet_len = rcp_datagram_serialize(&dgram, rx_buffer, RCP_TOTAL_SIZE);
    if (packet_len < 0)
        panic("[SEND] serialize failed, packet_len=%d\n", packet_len);

    if (verbose_p)
        trace("[SEND] sending segment %d (len=%d, packet_len=%d)\n", seg->seqno, seg->len,
              packet_len);
    nrf_send_ack(server, client_addr, rx_buffer, RCP_TOTAL_SIZE);
    sender_segment_sent(s, seg, curr_time);

    // Try to receive at client
    int ret = nrf_read_exact_timeout(client, rx_buffer, RCP_TOTAL_SIZE, timeout_usec);
    if (ret == RCP_TOTAL_SIZE) {
        // Process received packet
        struct rcp_datagram rx_dgram = rcp_datagram_init();
        if (rcp_datagram_parse(&rx_dgram, rx_buffer, RCP_TOTAL_SIZE) < 0) {
            panic("[RECV] corrupt packet received at client\n");
        }
        trace("[RECV] Received dgram with seqno %d and payload len %d\n", rx_dgram.header.seqno,
              rx_dgram.header.payload_len);

        // Process at receiver
        if (receiver_process_segment(r, &rx_dgram) == 0) {
            // Read the received data
            size_t available = receiver_bytes_available(r);
            if (available > 0) {
                size_t read = receiver_read(r, assembled + assembled_len, available);
                assembled_len += read;
                if (verbose_p)
                    trace("[RECV] received %d bytes: <%s>\n", read, assembled);
            }

            // Generate and send ACK back to server
            struct rcp_header ack = {0};
            receiver_get_ack(r, &ack);

            struct rcp_datagram ack_dgram = rcp_datagram_init();
            ack_dgram.header = ack;
            rcp_compute_checksum(&ack_dgram.header);

            int ack_len = rcp_datagram_serialize(&ack_dgram, rx_buffer, RCP_TOTAL_SIZE);
            if (verbose_p)
                trace("[RECV] sending ACK back to server\n");
            nrf_send_ack(client, server->rxaddr, rx_buffer, ack_len);
        }
    }

    // Try to receive ACK at server
    ret = nrf_read_exact_timeout(server, rx_buffer, RCP_TOTAL_SIZE, timeout_usec);
    if (ret == RCP_TOTAL_SIZE) {
        struct rcp_datagram ack_dgram = rcp_datagram_init();
        if (rcp_datagram_parse(&ack_dgram, rx_buffer, RCP_TOTAL_SIZE) < 0) {
            panic("[SEND] corrupt ACK received at server\n");
        }

        // Process ACK at sender
        int acked = sender_process_ack(s, &ack_dgram.header);
        if (acked > 0 && verbose_p) {
            trace("[SEND] processed ACK\n");
        }
    }

    // Verify received data
    if (memcmp(assembled, test_msg, msg_len) != 0)
        panic("data mismatch!\n");

    trace("[SEND] SUCCESS: transmitted packet and received ACK\n");
}

void notmain(void) {
    kmalloc_init(64);
    trace("Testing single packet TCP transmission over NRF\n");

    nrf_t *s = server_mk_ack(server_addr, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_ack(client_addr, RCP_TOTAL_SIZE);

    nrf_stat_start(s);
    nrf_stat_start(c);

    test_tcp_single_packet(s, c, 1);

    nrf_stat_print(s, "server: done with TCP test");
    nrf_stat_print(c, "client: done with TCP test");
}