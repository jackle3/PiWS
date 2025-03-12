#pragma once

#include "bytestream.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_PENDING_SEGMENTS 32  // Maximum number of out-of-order segments to store

// Structure to track a pending segment
struct pending_segment {
    uint8_t *data;              // Segment data
    size_t len;                 // Length of data
    uint16_t seqno;            // Starting sequence number
    bool received;             // Whether segment has been received
};

// Reassembler structure
struct reassembler {
    struct bytestream *output;  // Output stream for reassembled data
    uint16_t next_seqno;       // Next expected sequence number
    struct pending_segment segments[MAX_PENDING_SEGMENTS];  // Pending segments
    size_t capacity;           // Maximum bytes that can be buffered
    size_t bytes_pending;      // Total bytes currently buffered
};

// Initialize a new reassembler
struct reassembler* reassembler_init(struct bytestream *output, size_t capacity);

// Insert a new segment into the reassembler
// Returns number of bytes successfully inserted
size_t reassembler_insert(struct reassembler *r, const uint8_t *data, 
                         size_t len, uint16_t seqno, bool is_last);

// Get the next expected sequence number
uint16_t reassembler_next_seqno(const struct reassembler *r);

// Get number of bytes pending reassembly
size_t reassembler_bytes_pending(const struct reassembler *r);

// Check if reassembly is complete (all segments received and processed)
bool reassembler_is_complete(const struct reassembler *r);