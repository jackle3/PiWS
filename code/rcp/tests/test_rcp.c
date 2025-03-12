#include "rcp_datagram.h"
#include "rcp_header.h"
#include "rpi.h"

#include <stdio.h>
#include <string.h>

// Test RCP header initialization
static void test_header_init(void) {
    printk("--------------------------------\n");
    printk("Starting header initialization test...\n");
    struct rcp_header hdr = rcp_header_init();
    assert(hdr.payload_len == 0);
    assert(hdr.cksum == 0);
    assert(hdr.dst == 0);
    assert(hdr.src == 0);
    assert(hdr.seqno == 0);
    assert(hdr.flags == 0);
    assert(hdr.ackno == 0);
    assert(hdr.window == 0);
    printk("Header initialization test passed!\n");
    printk("--------------------------------\n");
}

// Test RCP header flag operations
static void test_header_flags(void) {
    printk("--------------------------------\n");
    printk("Starting header flags test...\n");
    struct rcp_header hdr = rcp_header_init();
    
    // Test setting flags
    rcp_set_flag(&hdr, RCP_FLAG_SYN);
    assert(rcp_has_flag(&hdr, RCP_FLAG_SYN));
    assert(!rcp_has_flag(&hdr, RCP_FLAG_ACK));
    
    rcp_set_flag(&hdr, RCP_FLAG_ACK);
    assert(rcp_has_flag(&hdr, RCP_FLAG_SYN));
    assert(rcp_has_flag(&hdr, RCP_FLAG_ACK));
    
    // Test clearing flags
    rcp_clear_flag(&hdr, RCP_FLAG_SYN);
    assert(!rcp_has_flag(&hdr, RCP_FLAG_SYN));
    assert(rcp_has_flag(&hdr, RCP_FLAG_ACK));
    
    printk("Header flag operations test passed!\n");
    printk("--------------------------------\n");
}

// Test RCP header serialization and parsing
static void test_header_serialization(void) {
    printk("--------------------------------\n");
    printk("Starting header serialization test...\n");
    struct rcp_header hdr = rcp_header_init();
    hdr.payload_len = 10;
    hdr.dst = 0x42;
    hdr.src = 0x24;
    hdr.seqno = 1234;
    rcp_set_flag(&hdr, RCP_FLAG_SYN | RCP_FLAG_ACK);
    hdr.ackno = 5678;
    hdr.window = 5;
    
    uint8_t buffer[RCP_HEADER_LENGTH];
    rcp_serialize(&hdr, buffer);
    
    struct rcp_header parsed = rcp_header_init();
    rcp_parse(&parsed, buffer);
    
    assert(parsed.payload_len == hdr.payload_len);
    assert(parsed.dst == hdr.dst);
    assert(parsed.src == hdr.src);
    assert(parsed.seqno == hdr.seqno);
    assert(parsed.flags == hdr.flags);
    assert(parsed.ackno == hdr.ackno);
    assert(parsed.window == hdr.window);
    
    printk("Header serialization/parsing test passed!\n");
    printk("--------------------------------\n");
}

// Test RCP datagram operations
static void test_datagram_operations(void) {
    printk("--------------------------------\n");
    printk("Starting datagram operations test...\n");
    struct rcp_datagram dgram = rcp_datagram_init();
    const char test_data[] = "Hello, RCP!";
    
    // Test payload setting
    assert(rcp_datagram_set_payload(&dgram, test_data, strlen(test_data)) == 0);
    assert(dgram.payload_length == strlen(test_data));
    assert(memcmp(dgram.payload, test_data, strlen(test_data)) == 0);
    
    // Test serialization
    uint8_t buffer[RCP_TOTAL_SIZE];
    int len = rcp_datagram_serialize(&dgram, buffer, sizeof(buffer));
    assert(len > 0);

    // Print out the serialized datagram
    printk("Serialized datagram: ");
    for (int i = 0; i < len; i++) {
        printk("%x ", buffer[i]);
    }
    printk("\n");
    
    // Test parsing
    struct rcp_datagram parsed = rcp_datagram_init();
    assert(rcp_datagram_parse(&parsed, buffer, len) == len);
    assert(parsed.payload_length == dgram.payload_length);
    assert(memcmp(parsed.payload, dgram.payload, parsed.payload_length) == 0);
    
    printk("Datagram operations test passed!\n");
    printk("--------------------------------\n");
}

// Test maximum payload size handling
static void test_max_payload(void) {
    printk("--------------------------------\n");
    printk("Starting max payload test...\n");
    struct rcp_datagram dgram = rcp_datagram_init();
    uint8_t max_payload[RCP_MAX_PAYLOAD];
    memset(max_payload, 'A', RCP_MAX_PAYLOAD);
    
    // Test setting maximum payload
    assert(rcp_datagram_set_payload(&dgram, max_payload, RCP_MAX_PAYLOAD) == 0);
    assert(dgram.payload_length == RCP_MAX_PAYLOAD);
    
    // Test exceeding maximum payload
    uint8_t too_large[RCP_MAX_PAYLOAD + 1];
    assert(rcp_datagram_set_payload(&dgram, too_large, sizeof(too_large)) == -1);

    printk("Maximum payload size test passed!\n");
    printk("--------------------------------\n");
}

// Test checksum computation
static void test_checksum(void) {
    printk("--------------------------------\n");
    printk("Starting checksum test...\n");
    struct rcp_header hdr = rcp_header_init();
    hdr.payload_len = 10;
    hdr.dst = 0x42;
    hdr.src = 0x24;
    
    // Compute initial checksum
    rcp_compute_checksum(&hdr);
    uint8_t original_checksum = hdr.cksum;
    
    // Modify a field and recompute
    hdr.dst = 0x43;
    rcp_compute_checksum(&hdr);
    printk("Checksum: %d\n", hdr.cksum);
    printk("Original checksum: %d\n", original_checksum);
    assert(hdr.cksum != original_checksum);
    
    printk("Checksum computation test passed!\n");
    printk("--------------------------------\n");
}

void notmain(void) {
    printk("Starting RCP tests...\n\n");

    kmalloc_init(1);
    
    test_header_init();
    test_header_flags();
    test_header_serialization();
    test_datagram_operations();
    test_max_payload();
    test_checksum();
    
    printk("\nAll RCP tests passed successfully!\n");
} 