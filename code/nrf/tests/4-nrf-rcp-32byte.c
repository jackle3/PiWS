#include "nrf.h"
#include "nrf-test.h"
#include "rcp_datagram.h"
#include <string.h>

// Test sending a 22-byte message in a single RCP packet
static void test_rcp_packet(nrf_t *n) {
    // Create test message that's exactly 22 bytes (RCP_MAX_PAYLOAD)
    const char test_msg[RCP_MAX_PAYLOAD] = "22-byte-test-message!!";
    
    // Create RCP datagram with our test message
    struct rcp_datagram dgram = rcp_datagram_init();
    
    // Set up header fields
    dgram.header.src = 0x01;      // Source address 1
    dgram.header.dst = 0x02;      // Destination address 2
    dgram.header.seqno = 1;       // First packet
    dgram.header.window = 1;      // Window size of 1
    rcp_set_flag(&dgram.header, RCP_FLAG_SYN);  // SYN packet
    
    // Set the payload
    if (rcp_datagram_set_payload(&dgram, test_msg, sizeof(test_msg)) < 0) {
        panic("Failed to set payload\n");
    }
    
    // Compute checksum
    rcp_compute_checksum(&dgram.header);
    
    // Buffer for serialized packet
    uint8_t packet[RCP_TOTAL_SIZE];
    int packet_len = rcp_datagram_serialize(&dgram, packet, sizeof(packet));
    if (packet_len < 0) {
        panic("Failed to serialize packet\n");
    }
    
    // Verify packet size
    if (packet_len != RCP_TOTAL_SIZE) {
        panic("Unexpected packet size: got %d, expected %d\n", 
              packet_len, RCP_TOTAL_SIZE);
    }
    
    // Send the packet
    output("Sending RCP packet (header=%d bytes, payload=%d bytes, total=%d bytes)\n",
           RCP_HEADER_LENGTH, dgram.payload_length, packet_len);
    nrf_send_ack(n, 0x12345678, packet, packet_len);
    output("Packet sent!\n");
    
    // Wait for acknowledgment
    uint8_t rx[RCP_TOTAL_SIZE];
    int rx_len = nrf_read_exact(n, rx, packet_len);
    if (rx_len < 0) {
        panic("Failed to get ACK\n");
    }
    
    // Parse received packet
    struct rcp_datagram rx_dgram = rcp_datagram_init();
    if (rcp_datagram_parse(&rx_dgram, rx, rx_len) < 0) {
        panic("Failed to parse received packet\n");
    }
    
    // Verify received header fields
    if (rx_dgram.header.src != dgram.header.src ||
        rx_dgram.header.dst != dgram.header.dst ||
        rx_dgram.header.seqno != dgram.header.seqno ||
        rx_dgram.header.window != dgram.header.window ||
        !rcp_has_flag(&rx_dgram.header, RCP_FLAG_SYN)) {
        panic("Header field mismatch!\n");
    }
    
    // Verify received payload
    if (rx_dgram.payload_length != sizeof(test_msg)) {
        panic("Received wrong payload size: got %zu, expected %zu\n",
              rx_dgram.payload_length, sizeof(test_msg));
    }
    
    if (memcmp(rx_dgram.payload, test_msg, sizeof(test_msg)) != 0) {
        panic("Received data doesn't match sent data\n");
    }
    
    output("Successfully received ACK with matching header and payload!\n");
    
    // Clean up
    rcp_datagram_free(&dgram);
    rcp_datagram_free(&rx_dgram);
}

void notmain(void) {
    output("Testing RCP packet transmission over NRF\n");
    
    // Initialize NRF
    nrf_t *n = nrf_init_test_default();
    if (!n) {
        panic("Failed to initialize NRF\n");
    }
    
    test_rcp_packet(n);
    
    output("All tests passed!\n");
} 