#include "nrf.h"
#include "nrf-test.h"
#include "rcp-datagram.h"
#include "../sender.h"
#include "../receiver.h"
#include <string.h>

// Test sending a large message that needs to be split into multiple segments
static void test_tcp_over_nrf(nrf_t *n) {
    // Create sender and receiver
    struct sender *s = sender_init(0x01, 0x02, 1000);
    struct receiver *r = receiver_init(0x02, 0x01);
    if (!s || !r) panic("Failed to initialize sender/receiver\n");

    // Create test message larger than RCP_MAX_PAYLOAD
    const char *test_msg = "This is a long test message that will be split into "
                          "multiple segments to demonstrate TCP-like reliable "
                          "transmission over NRF using RCP packets!";
    size_t msg_len = strlen(test_msg);

    // Write message to sender's outgoing stream
    size_t written = bytestream_write(s->outgoing, (uint8_t*)test_msg, msg_len);
    if (written != msg_len) panic("Failed to write test message\n");

    // Fill sender's window with segments
    int segments = sender_fill_window(s);
    output("Created %d segments from message\n", segments);

    uint32_t current_time = 0;  // Simple time counter for retransmission
    bool done = false;
    uint8_t rx_buffer[RCP_TOTAL_SIZE];
    uint8_t assembled[1000];
    size_t assembled_len = 0;

    while (!done) {
        // Get next segment to send
        const struct unacked_segment *seg = sender_next_segment(s);
        if (seg) {
            // Create RCP datagram for segment
            struct rcp_datagram dgram = rcp_datagram_init();
            dgram.header.src = s->src_addr;
            dgram.header.dst = s->dst_addr;
            dgram.header.seqno = seg->seqno;
            dgram.header.window = SENDER_WINDOW_SIZE;
            
            // Set payload
            if (rcp_datagram_set_payload(&dgram, seg->data, seg->len) < 0) {
                panic("Failed to set payload\n");
            }

            // Compute checksum
            rcp_compute_checksum(&dgram.header);

            // Serialize and send packet
            uint8_t packet[RCP_TOTAL_SIZE];
            int packet_len = rcp_datagram_serialize(&dgram, packet, sizeof(packet));
            if (packet_len < 0) panic("Failed to serialize packet\n");

            output("Sending segment %d (len=%d)\n", seg->seqno, seg->len);
            nrf_send_ack(n, 0x12345678, packet, packet_len);
            
            // Mark segment as sent
            sender_segment_sent(s, seg, current_time);

            // Wait for ACK
            int rx_len = nrf_read_exact(n, rx_buffer, packet_len);
            if (rx_len < 0) panic("Failed to get ACK\n");

            // Parse received packet
            struct rcp_datagram rx_dgram = rcp_datagram_init();
            if (rcp_datagram_parse(&rx_dgram, rx_buffer, rx_len) < 0) {
                panic("Failed to parse received packet\n");
            }

            // Process ACK
            if (rcp_has_flag(&rx_dgram.header, RCP_FLAG_ACK)) {
                int acked = sender_process_ack(s, &rx_dgram.header);
                output("Received ACK for %d segments\n", acked);

                // Read any available data from receiver
                size_t available = receiver_bytes_available(r);
                if (available > 0) {
                    size_t read = receiver_read(r, 
                                             assembled + assembled_len,
                                             available);
                    assembled_len += read;
                }
            }

            rcp_datagram_free(&dgram);
            rcp_datagram_free(&rx_dgram);
        }

        // Check for retransmissions
        int retrans = sender_check_retransmit(s, current_time);
        if (retrans > 0) {
            output("Marking %d segments for retransmission\n", retrans);
        }

        // Fill window with new segments if possible
        segments = sender_fill_window(s);
        if (segments > 0) {
            output("Created %d new segments\n", segments);
        }

        // Check if we're done
        done = (assembled_len == msg_len);
        current_time += 100;  // Simple time increment
    }

    // Verify received data
    if (memcmp(assembled, test_msg, msg_len) != 0) {
        panic("Received data doesn't match sent data!\n");
    }

    output("Successfully transmitted message: %s\n", assembled);

    sender_free(s);
    receiver_free(r);
}

void notmain(void) {
    output("Testing TCP-like transmission over NRF\n");
    
    // Initialize NRF
    nrf_t *n = nrf_init_test_default();
    if (!n) {
        panic("Failed to initialize NRF\n");
    }
    
    test_tcp_over_nrf(n);
    
    output("All tests passed!\n");
} 