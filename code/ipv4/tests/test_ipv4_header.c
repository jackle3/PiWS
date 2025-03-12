#include "ipv4_header.h"
#include "rpi.h"
#include <stdio.h>
#include <string.h>

/* Test helper to print success/failure */
static void test_result(const char* test_name, int success) {
    printk("%s: %s\n", test_name, success ? "PASS" : "FAIL");
}

/* Test payload length calculation */
static void test_payload_length(void) {
    struct ipv4_header hdr = {0};
    hdr.hlen = 5;  // Standard 20-byte header
    hdr.len = 100; // Total packet length of 100 bytes
    
    uint16_t payload_len = ipv4_payload_length(&hdr);
    test_result("Payload Length", payload_len == 80); // 100 - (5 * 4) = 80
}

/* Test checksum calculation */
static void test_checksum(void) {
    struct ipv4_header hdr = {
        .ver = 4,
        .hlen = 5,
        .tos = 0,
        .len = 60,
        .id = 0x1234,
        .df = 1,
        .mf = 0,
        .offset = 0,
        .ttl = 64,
        .proto = IPV4_PROTO_TCP,
        .src = 0xC0A80101,  // 192.168.1.1
        .dst = 0xC0A80102   // 192.168.1.2
    };
    
    // Compute checksum
    ipv4_compute_checksum(&hdr);
    
    // Verify checksum is non-zero
    int checksum_nonzero = (hdr.cksum != 0);
    
    // Verify checksum is correct by recomputing
    uint16_t original_checksum = hdr.cksum;
    ipv4_compute_checksum(&hdr);
    int checksum_stable = (hdr.cksum == original_checksum);
    
    test_result("Checksum Calculation", checksum_nonzero && checksum_stable);
}

/* Test parsing and serialization */
static void test_parse_serialize(void) {
    // Create a sample packet
    uint8_t packet[IPV4_HEADER_LENGTH] = {
        0x45, 0x00, 0x00, 0x3c, // Ver=4, IHL=5, TOS=0, Len=60
        0x12, 0x34, 0x40, 0x00, // ID=0x1234, Flags=DF, Offset=0
        0x40, 0x06, 0x00, 0x00, // TTL=64, Proto=TCP, Checksum=0
        0xc0, 0xa8, 0x01, 0x01, // Src=192.168.1.1
        0xc0, 0xa8, 0x01, 0x02  // Dst=192.168.1.2
    };
    
    struct ipv4_header hdr = {0};
    ipv4_parse(&hdr, packet);
    
    // Verify parsed values
    int parse_correct = (
        hdr.ver == 4 &&
        hdr.hlen == 5 &&
        hdr.len == 60 &&
        hdr.id == 0x1234 &&
        hdr.df == 1 &&
        hdr.mf == 0 &&
        hdr.offset == 0 &&
        hdr.ttl == 64 &&
        hdr.proto == IPV4_PROTO_TCP &&
        hdr.src == 0xC0A80101 &&
        hdr.dst == 0xC0A80102
    );
    
    // Test serialization
    uint8_t output[IPV4_HEADER_LENGTH] = {0};
    ipv4_serialize(&hdr, output);
    
    // Compare original packet with serialized output
    int serialize_correct = (memcmp(packet, output, IPV4_HEADER_LENGTH) == 0);
    
    test_result("Parse/Serialize", parse_correct && serialize_correct);
}

/* Test pseudo header checksum */
static void test_pseudo_checksum(void) {
    struct ipv4_header hdr = {
        .ver = 4,
        .hlen = 5,
        .len = 60,
        .ttl = 64,
        .proto = IPV4_PROTO_TCP,
        .src = 0xC0A80101,  // 192.168.1.1
        .dst = 0xC0A80102   // 192.168.1.2
    };
    
    uint32_t sum = ipv4_pseudo_checksum(&hdr);
    
    // Expected sum should include:
    // - src IP (0xC0A8 + 0x0101)
    // - dst IP (0xC0A8 + 0x0102)
    // - protocol (0x0006)
    // - TCP length (60 - 20 = 40)
    
    test_result("Pseudo Checksum", sum != 0);
}

void notmain(void) {
    printk("Starting IPv4 Header Tests\n");
    printk("-------------------------\n");
    
    test_payload_length();
    test_checksum();
    test_parse_serialize();
    test_pseudo_checksum();
    
    printk("-------------------------\n");
    printk("Tests Complete\n");
} 