#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bytestream.h"

#define RECEIVER_WINDOW_SIZE 32  // Maximum number of segments in the reassembler

// Structure to hold a pending segment
struct pending_segment {
    uint8_t *data;          // Segment data
    size_t len;            // Length of data
    bool received;         // Whether this slot contains data
};

// Reassembler structure
struct reassembler {
    struct bytestream *output;    // Output stream for reassembled data
    uint16_t next_seqno;         // Next sequence number expected
    size_t capacity;             // Maximum bytes that can be buffered
    size_t bytes_pending;        // Current bytes pending in buffer
    struct pending_segment segments[RECEIVER_WINDOW_SIZE];  // Fixed window of segments
};

// Initialize a new reassembler
struct reassembler* reassembler_init(struct bytestream *out_stream, size_t capacity);

// Insert a new segment into the reassembler
// Returns number of bytes inserted, or 0 if segment couldn't be inserted
size_t reassembler_insert(struct reassembler *r, const uint8_t *data, size_t len, uint16_t seqno, bool is_last);

// Get next expected sequence number
uint16_t reassembler_next_seqno(const struct reassembler *r);

// Get number of bytes pending in buffer
size_t reassembler_bytes_pending(const struct reassembler *r);

// Check if reassembly is complete
bool reassembler_is_complete(const struct reassembler *r);