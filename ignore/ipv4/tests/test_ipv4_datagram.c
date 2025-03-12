#include "ipv4_datagram.h"
#include <stdio.h>
#include <string.h>

static void test_result(const char* test_name, int success) {
    printk("%s: %s\n", test_name, success ? "PASS" : "FAIL");
}

static void test_datagram_init(void) {
    struct ipv4_datagram dgram = ipv4_datagram_init();
    
    int init_correct = (
        dgram.header.ver == 4 &&
        dgram.header.hlen == 5 &&
        dgram.payload == NULL &&
        dgram.payload_length == 0
    );
    
    test_result("Datagram Init", init_correct);
}

static void test_datagram_payload(void) {
    struct ipv4_datagram dgram = ipv4_datagram_init();
    
    // Test payload setting
    const char* test_data = "Hello, IPv4!";
    size_t test_len = strlen(test_data);
    
    int set_result = ipv4_datagram_set_payload(&dgram, test_data, test_len);
    
    int payload_correct = (
        set_result == 0 &&
        dgram.payload_length == test_len &&
        dgram.header.len == IPV4_HEADER_LENGTH + test_len &&
        memcmp(dgram.payload, test_data, test_len) == 0
    );
    
    test_result("Datagram Payload", payload_correct);
}

static void test_datagram_serialize_parse(void) {
    struct ipv4_datagram orig = ipv4_datagram_init();
    
    // Set up test data
    const char* test_data = "Test payload";
    ipv4_datagram_set_payload(&orig, test_data, strlen(test_data));
    orig.header.src = 0xC0A80101;  // 192.168.1.1
    orig.header.dst = 0xC0A80102;  // 192.168.1.2
    
    // Serialize
    uint8_t buffer[1024];
    int serialized_len = ipv4_datagram_serialize(&orig, buffer, sizeof(buffer));
    
    // Parse back
    struct ipv4_datagram parsed = ipv4_datagram_init();
    int parsed_len = ipv4_datagram_parse(&parsed, buffer, serialized_len);
    
    // Verify
    int serialize_parse_correct = (
        serialized_len == parsed_len &&
        parsed.header.src == orig.header.src &&
        parsed.header.dst == orig.header.dst &&
        parsed.payload_length == orig.payload_length &&
        memcmp(parsed.payload, orig.payload, orig.payload_length) == 0
    );

    test_result("Datagram Serialize/Parse", serialize_parse_correct);
}

void notmain(void) {
    kmalloc_init(64);
    
    printk("Starting IPv4 Datagram Tests\n");
    printk("-------------------------\n");
    
    test_datagram_init();
    test_datagram_payload();
    test_datagram_serialize_parse();
    
    printk("-------------------------\n");
    printk("Tests Complete\n");
} 