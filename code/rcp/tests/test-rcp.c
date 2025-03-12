// Test checksum computation
static void test_checksum(void) {
    struct rcp_header hdr1 = rcp_header_init();
    struct rcp_header hdr2 = rcp_header_init();
    
    // Set same initial values
    hdr1.payload_len = 10;
    hdr1.dst = 0x42;
    hdr1.src = 0x24;
    hdr1.seqno = 1234;
    
    hdr2.payload_len = 10;
    hdr2.dst = 0x42;
    hdr2.src = 0x24;
    hdr2.seqno = 1234;
    
    // Compute checksums
    rcp_compute_checksum(&hdr1);
    rcp_compute_checksum(&hdr2);
    
    // Verify same data produces same checksum
    assert(hdr1.cksum == hdr2.cksum);
    uint8_t first_checksum = hdr1.cksum;
    
    // Modify data in second header
    hdr2.dst = 0x43;  // Change destination
    rcp_compute_checksum(&hdr2);
    
    // Verify different data produces different checksum
    assert(hdr2.cksum != first_checksum);
    
    // Verify checksum is stable (computing it again on same data gives same result)
    uint8_t prev_checksum = hdr2.cksum;
    rcp_compute_checksum(&hdr2);
    assert(hdr2.cksum == prev_checksum);
    
    printf("Checksum computation test passed!\n");
} 