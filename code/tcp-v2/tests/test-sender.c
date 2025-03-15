#include <stdio.h>
#include <string.h>

#include "sender.h"

// Mock NRF for testing
typedef struct mock_nrf {
    // Any state needed for the mock
    int dummy;
} mock_nrf_t;

mock_nrf_t mock_nrf_init() {
    mock_nrf_t nrf = {0};
    return nrf;
}

// Track the last segment transmitted
static sender_segment_t last_segment;
static int segment_count = 0;

// Mock transmit callback for sender
static void mock_transmit(tcp_peer_t *peer, sender_segment_t *segment) {
    printk("Mock transmit: seqno=%u, len=%u, is_syn=%d, is_fin=%d: %s\n", segment->seqno,
           segment->len, segment->is_syn, segment->is_fin, segment->payload);

    // Save a copy of the segment for later inspection
    memcpy(&last_segment, segment, sizeof(sender_segment_t));
    segment_count++;
}

// Test sender functionality
static void test_sender(void) {
    printk("--------------------------------\n");
    printk("Starting sender test...\n");

    // Initialize mock NRF
    mock_nrf_t mock_nrf = mock_nrf_init();

    // Initialize sender
    sender_t sender = sender_init((nrf_t *)&mock_nrf, mock_transmit, NULL);
    printk("Sender initialized\n");

    // Write test data to the sender's bytestream
    const char *test_data = "This is a test message that will be split into multiple segments";
    size_t len = strlen(test_data);
    size_t written = bs_write(&sender.reader, (uint8_t *)test_data, len);
    assert(written == len);
    printk("Wrote %u bytes to sender's bytestream: '%s'\n", written, test_data);

    // Push data to be sent
    printk("--------------------------------\n");
    while (sender.next_seqno < len) {
        printk("Pushing data to send (should trigger segment creation)...\n");
        uint16_t remaining_space = sender.acked_seqno + sender.window_size - sender.next_seqno;
        while (remaining_space) {
            sender_push(&sender);
            remaining_space = sender.acked_seqno + sender.window_size - sender.next_seqno;
            printk("  Pushed data... next_seqno: %u, remaining_space: %u, bytes_popped: %u\n",
                   sender.next_seqno, remaining_space, bs_bytes_popped(&sender.reader));

            if (!bs_bytes_available(&sender.reader)) {
                printk("No more data to push\n");
                break;
            }
        }

        // Verify that data was pushed
        assert(sender.next_seqno > 0);
        assert(!rtq_empty(&sender.pending_segs));
        printk("Segments created and transmitted. Next seqno: %u, Window size: %u\n",
               sender.next_seqno, sender.window_size);

        printk("--------------------------------\n");
        // Create mock ACK from receiver
        receiver_segment_t reply = {0};
        reply.is_ack = true;
        reply.ackno = sender.next_seqno;  // ACK everything sent so far
        reply.window_size = 64;           // Increase window size

        // Process the ACK
        printk("Processing ACK with ackno=%u\n", reply.ackno);
        sender_process_reply(&sender, &reply);

        // Verify ACK was processed
        assert(sender.acked_seqno == reply.ackno);
        assert(sender.window_size == reply.window_size);
        assert(rtq_empty(&sender.pending_segs));  // All segments should be ACKed
        printk("ACK processed. Acked seqno: %u, Window size: %u\n", sender.acked_seqno,
               sender.window_size);
        printk("--------------------------------\n");
    }
    printk("--------------------------------\n");

    // Test retransmission mechanism
    printk("Testing retransmission mechanism...\n");

    // Write more data and push without ACKing
    const char *more_data = "More bytes for retransmission test";
    written = bs_write(&sender.reader, (uint8_t *)more_data, strlen(more_data));
    sender_push(&sender);
    printk("  Pushed data... next_seqno: %u, bytes_popped: %u\n", sender.next_seqno,
           bs_bytes_popped(&sender.reader));

    // Force retransmission time
    sender.rto_time_us = timer_get_usec() - S_TO_US(5);  // Set RTO time to past

    // Check retransmits
    printk("Checking for retransmits (should trigger retransmission)...\n");
    sender_check_retransmits(&sender);

    // Verify retransmission counter increased
    assert(sender.n_retransmits > 0);
    printk("Retransmission counter: %u\n", sender.n_retransmits);

    // Reset segment tracking before testing FIN behavior
    segment_count = 0;
    memset(&last_segment, 0, sizeof(last_segment));
    printk("--------------------------------\n");

    // End the stream
    printk("Ending input stream...\n");
    bs_end_input(&sender.reader);

    // Keep track of how many bytes were written to check if we've sent everything
    size_t total_bytes = bs_bytes_available(&sender.reader);

    printk("Pushing data until all bytes and FIN are sent...\n");
    printk("Total bytes to send: %u\n", total_bytes);

    // Keep calling sender_push until all data is sent and FIN is observed
    bool fin_observed = false;
    uint16_t remaining_space = sender.acked_seqno + sender.window_size - sender.next_seqno;
    while (remaining_space) {
        sender_push(&sender);
        remaining_space = sender.acked_seqno + sender.window_size - sender.next_seqno;
        printk("  Pushed data... next_seqno: %u, remaining_space: %u, bytes_popped: %u\n",
               sender.next_seqno, remaining_space, bs_bytes_popped(&sender.reader));

        if (last_segment.is_fin) {
            fin_observed = true;
            printk("FIN flag observed on (seqno=%u)\n", last_segment.seqno);
        }

        if (!bs_bytes_available(&sender.reader)) {
            printk("No more data to push\n");
            break;
        }
    }

    // Verify that we saw a FIN flag
    assert(fin_observed);
    printk("Verified FIN flag was set on the final segment\n");

    // Print final state
    printk("Final state - bs_eof: %d, bs_bytes_available: %u, bs_bytes_written: %u\n",
           sender.reader.eof, bs_bytes_available(&sender.reader), bs_bytes_written(&sender.reader));

    // Check for pending segments with FIN
    assert(!rtq_empty(&sender.pending_segs));
    printk("Stream ended. All data sent with FIN flag.\n");

    printk("Sender test passed!\n");
    printk("--------------------------------\n");
}

void notmain(void) {
    printk("Starting TCP implementation tests...\n\n");
    kmalloc_init(64);
    printk("Memory initialized\n");

    test_sender();

    printk("\nSender test passed!\n");
}