#pragma once

#include "bytestream.h"
#include "rcp-datagram.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENDER_WINDOW_SIZE 8  // Maximum number of unacked segments

// Structure to track unacknowledged segments
struct unacked_segment {
    uint8_t data[RCP_MAX_PAYLOAD];  // Segment data
    size_t len;                     // Length of data
    uint16_t seqno;                 // Sequence number
    uint32_t send_time;            // Time when segment was sent
    bool acked;                    // Whether segment has been acknowledged
};

// Sender structure
struct sender {
    struct bytestream *outgoing;    // Outgoing bytestream
    uint16_t next_seqno;           // Next sequence number to use
    uint16_t window_size;          // Current window size (flow control)
    struct unacked_segment segments[SENDER_WINDOW_SIZE];  // Unacked segments
    size_t segments_in_flight;     // Number of unacked segments
    uint8_t src_addr;             // Source address
    uint8_t dst_addr;             // Destination address
};

// Initialize a new sender
struct sender* sender_init(uint8_t src_addr, uint8_t dst_addr, size_t stream_capacity);

// Fill window with new segments if possible
// Returns number of new segments created
int sender_fill_window(struct sender *s);

// Process an acknowledgment
// Returns number of newly acknowledged segments
int sender_process_ack(struct sender *s, const struct rcp_header *ack);

// Check for segments that need retransmission
// current_time_ms should be monotonically increasing
// Returns number of segments marked for retransmission
// int sender_check_retransmit(struct sender *s, uint32_t current_time_ms);

// Get next segment to transmit (either new or retransmission)
// Returns NULL if no segment available
const struct unacked_segment* sender_next_segment(const struct sender *s);

// Mark a segment as sent
void sender_segment_sent(struct sender *s, const struct unacked_segment *seg, uint32_t current_time_ms);