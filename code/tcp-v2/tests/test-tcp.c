#include <stdio.h>
#include <string.h>

#include "bytestream.h"
// #include "reassembler.h"
// #include "receiver.h"
// #include "sender.h"

// Test basic bytestream operations
static void test_bytestream(void) {
    printk("--------------------------------\n");
    printk("Starting bytestream test...\n");
    struct bytestream bs;
    bs_init(&bs);
    assert(&bs != NULL);
    printk("Bytestream initialized with capacity 100\n");

    const char *test_data = "Hello, TCP!";
    size_t len = strlen(test_data);

    // Test writing
    size_t written = bs_write(&bs, (uint8_t *)test_data, len);
    assert(written == len);
    assert(bs_bytes_available(&bs) == len);
    printk("Successfully wrote %u bytes: '%s'\n", written, test_data);

    // Test reading
    uint8_t buffer[20];
    size_t read = bs_read(&bs, buffer, sizeof(buffer));
    assert(read == len);
    assert(memcmp(buffer, test_data, len) == 0);
    buffer[read] = '\0';
    printk("Successfully read %u bytes: '%s'\n", read, buffer);

    printk("Bytestream test passed!\n");
    printk("--------------------------------\n");
}

// // Test sender functionality
// static void test_sender(void) {
//     printk("--------------------------------\n");
//     printk("Starting sender test...\n");
//     struct sender *s = sender_init(0x1, 0x2, 1000);
//     assert(s != NULL);
//     printk("Sender initialized with src=0x1, dst=0x2\n");

//     // Write some test data
//     const char *test_data = "This is a test message that will be split into multiple segments";
//     size_t len = strlen(test_data);
//     size_t written = bytestream_write(s->outgoing, (uint8_t*)test_data, len);
//     assert(written == len);
//     printk("Wrote %u bytes to sender: '%s'\n", written, test_data);

//     // Fill the window with segments
//     int segments = sender_fill_window(s);
//     assert(segments > 0);
//     assert(s->segments_in_flight == (size_t)segments);
//     printk("Created %d segments to fill window\n", segments);

//     // Test getting next segment
//     const struct unacked_segment *seg = sender_next_segment(s);
//     assert(seg != NULL);
//     assert(seg->seqno == 0);  // First segment
//     assert(seg->len > 0 && seg->len <= RCP_MAX_PAYLOAD);
//     printk("Got next segment: seqno=%u, len=%u\n", seg->seqno, seg->len);

//     // Mark segment as sent
//     sender_segment_sent(s, seg, 1000);  // 1000ms timestamp
//     printk("Marked segment as sent at t=1000ms\n");

//     // Create an ACK
//     struct rcp_header ack = {0};
//     ack.ackno = 0;  // ACK first segment
//     ack.window = SENDER_WINDOW_SIZE;

//     // Process ACK
//     int acked = sender_process_ack(s, &ack);
//     assert(acked == 1);
//     assert(s->segments_in_flight == (size_t)(segments - 1));
//     printk("Processed ACK: %d segments acknowledged\n", acked);

//     printk("Sender test passed!\n");
//     printk("--------------------------------\n");
// }

// // Test receiver functionality
// static void test_receiver(void) {
//     printk("--------------------------------\n");
//     printk("Starting receiver test...\n");
//     struct receiver *r = receiver_init(0x2, 0x1);
//     assert(r != NULL);
//     printk("Receiver initialized with src=0x2, dst=0x1\n");

//     // Create a test segment
//     struct rcp_datagram dgram = {0};
//     const char *test_data = "Test segment data";
//     size_t len = strlen(test_data);

//     dgram.header.src = 0x1;
//     dgram.header.dst = 0x2;
//     dgram.header.seqno = 0;
//     dgram.payload = (uint8_t*)test_data;
//     dgram.header.payload_len = len;
//     printk("Created test segment with data: '%s'\n", test_data);

//     // Process the segment
//     int result = receiver_process_segment(r, &dgram);
//     assert(result == 0);
//     assert(receiver_bytes_available(r) == len);
//     printk("Successfully processed segment, %u bytes available\n", len);

//     // Read the data
//     uint8_t buffer[100];
//     size_t read = receiver_read(r, buffer, sizeof(buffer));
//     assert(read == len);
//     assert(memcmp(buffer, test_data, len) == 0);
//     buffer[read] = '\0';
//     printk("Read %u bytes from receiver: '%s'\n", read, buffer);

//     // Get ACK information
//     struct rcp_header ack = {0};
//     receiver_get_ack(r, &ack);
//     assert(ack.ackno == 0);  // ACK for first segment
//     assert(rcp_has_flag(&ack, RCP_FLAG_ACK));
//     printk("Generated ACK with ackno=%u\n", ack.ackno);

//     printk("Receiver test passed!\n");
//     printk("--------------------------------\n");
// }

// // Test out-of-order packet handling
// static void test_out_of_order(void) {
//     printk("--------------------------------\n");
//     printk("Starting out-of-order test...\n");
//     struct receiver *r = receiver_init(0x2, 0x1);
//     assert(r != NULL);
//     printk("Receiver initialized with src=0x2, dst=0x1\n");

//     // Create test segments
//     const char *data1 = "First segment";
//     const char *data2 = "Second segment";
//     const char *data3 = "Third segment";

//     struct rcp_datagram dgram1 = {0}, dgram2 = {0}, dgram3 = {0};

//     // Set up segment 2 (sending out of order)
//     dgram2.header.src = 0x1;
//     dgram2.header.dst = 0x2;
//     dgram2.header.seqno = 1;
//     dgram2.payload = (uint8_t*)data2;
//     dgram2.header.payload_len = strlen(data2);

//     // Process segment 2
//     printk("Processing segment 2 first (out of order)\n");
//     int result = receiver_process_segment(r, &dgram2);
//     assert(result == 0);
//     assert(receiver_bytes_available(r) == 0);  // Nothing ready yet
//     printk("Segment 2 buffered, no bytes available yet\n");

//     // Now send segment 1
//     dgram1.header.src = 0x1;
//     dgram1.header.dst = 0x2;
//     dgram1.header.seqno = 0;
//     dgram1.payload = (uint8_t*)data1;
//     dgram1.header.payload_len = strlen(data1);

//     printk("Processing segment 1\n");
//     result = receiver_process_segment(r, &dgram1);
//     assert(result == 0);
//     assert(receiver_bytes_available(r) == strlen(data1) + strlen(data2));
//     printk("Segments 1 and 2 now available (%u bytes)\n", strlen(data1) + strlen(data2));

//     // Send segment 3
//     dgram3.header.src = 0x1;
//     dgram3.header.dst = 0x2;
//     dgram3.header.seqno = 2;
//     dgram3.payload = (uint8_t*)data3;
//     dgram3.header.payload_len = strlen(data3);

//     printk("Processing segment 3\n");
//     result = receiver_process_segment(r, &dgram3);
//     assert(result == 0);

//     // Read all data
//     uint8_t buffer[100];
//     size_t total_len = strlen(data1) + strlen(data2) + strlen(data3);
//     size_t read = receiver_read(r, buffer, sizeof(buffer));
//     assert(read == total_len);
//     printk("Read all %u bytes of reassembled data\n", read);

//     // Verify the data is in order
//     size_t pos = 0;
//     assert(memcmp(buffer + pos, data1, strlen(data1)) == 0);
//     pos += strlen(data1);
//     assert(memcmp(buffer + pos, data2, strlen(data2)) == 0);
//     pos += strlen(data2);
//     assert(memcmp(buffer + pos, data3, strlen(data3)) == 0);
//     printk("Verified data is correctly ordered\n");

//     printk("Out-of-order handling test passed!\n");
//     printk("--------------------------------\n");
// }

// // Test reassembler functionality
// static void test_reassembler(void) {
//     printk("--------------------------------\n");
//     printk("Starting reassembler test...\n");
//     struct bytestream *bs = bytestream_init(1000);
//     assert(bs != NULL);
//     printk("Bytestream initialized with capacity 1000\n");

//     struct reassembler *r = reassembler_init(bs, 1000);
//     assert(r != NULL);
//     printk("Reassembler initialized with capacity 1000\n");

//     // Test inserting segments in order
//     const char *data1 = "First";
//     const char *data2 = "Second";
//     const char *data3 = "Third";

//     // Insert first segment
//     size_t len1 = strlen(data1);
//     size_t inserted = reassembler_insert(r, (uint8_t*)data1, len1, 0, false);
//     assert(inserted == len1);
//     assert(bytestream_bytes_available(bs) == len1);
//     printk("Inserted first segment: '%s'\n", data1);

//     // Insert second segment
//     size_t len2 = strlen(data2);
//     inserted = reassembler_insert(r, (uint8_t*)data2, len2, 1, false);
//     assert(inserted == len2);
//     assert(bytestream_bytes_available(bs) == len1 + len2);
//     printk("Inserted second segment: '%s'\n", data2);

//     // Insert third segment
//     size_t len3 = strlen(data3);
//     inserted = reassembler_insert(r, (uint8_t*)data3, len3, 2, true);
//     assert(inserted == len3);
//     assert(bytestream_bytes_available(bs) == len1 + len2 + len3);
//     printk("Inserted third segment: '%s'\n", data3);

//     // Read and verify the reassembled data
//     uint8_t buffer[1000];
//     size_t total_len = len1 + len2 + len3;
//     size_t read = bytestream_read(bs, buffer, sizeof(buffer));
//     assert(read == total_len);
//     printk("Read %u bytes of reassembled data\n", read);

//     // Verify the data is in order
//     size_t pos = 0;
//     assert(memcmp(buffer + pos, data1, len1) == 0);
//     pos += len1;
//     assert(memcmp(buffer + pos, data2, len2) == 0);
//     pos += len2;
//     assert(memcmp(buffer + pos, data3, len3) == 0);
//     printk("Verified ordered segments are correctly assembled\n");

//     // Test out-of-order insertion
//     const char *data4 = "Fourth";
//     const char *data5 = "Fifth";
//     size_t len4 = strlen(data4);
//     size_t len5 = strlen(data5);

//     // Insert segment 5 first
//     inserted = reassembler_insert(r, (uint8_t*)data5, len5, 4, false);
//     assert(inserted == len5);
//     assert(bytestream_bytes_available(bs) == 0);  // Not ready yet
//     printk("Inserted fifth segment out of order: '%s'\n", data5);

//     // Insert segment 4
//     inserted = reassembler_insert(r, (uint8_t*)data4, len4, 3, false);
//     assert(inserted == len4);
//     assert(bytestream_bytes_available(bs) == len4 + len5);
//     printk("Inserted fourth segment: '%s'\n", data4);

//     // Read and verify the new data
//     read = bytestream_read(bs, buffer, sizeof(buffer));
//     assert(read == len4 + len5);
//     assert(memcmp(buffer, data4, len4) == 0);
//     assert(memcmp(buffer + len4, data5, len5) == 0);
//     printk("Verified out-of-order segments are correctly assembled\n");

//     printk("Reassembler test passed!\n");
//     printk("--------------------------------\n");
// }

void notmain(void) {
    printk("Starting TCP implementation tests...\n\n");
    kmalloc_init(64);
    printk("Memory initialized\n");

    test_bytestream();
    // test_sender();
    // test_receiver();
    // test_out_of_order();
    // test_reassembler();

    printk("\nAll TCP implementation tests passed successfully!\n");
}