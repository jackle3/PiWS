#pragma once

#include "bytestream.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum number of out-of-order segments that can be stored
#define MAX_PENDING_SEGMENTS 32

// A slot represents a position in the reassembler's buffer where an out-of-order 
// segment can be stored. Each slot tracks:
// - The actual data bytes of the segment
// - The length of the segment
// - The sequence number (position in the byte stream)
// - Whether this slot contains valid data
struct pending_segment {
    uint8_t *data;              // The actual bytes of the segment
    size_t len;                 // Number of bytes in this segment
    uint16_t seqno;            // Where these bytes belong in the overall stream
    bool received;             // Whether this slot contains valid data
};

// The reassembler maintains an ordered buffer of segments and tracks:
// - Which sequence number we expect next (for in-order delivery)
// - How many bytes we can buffer (capacity)
// - How many bytes are currently buffered (bytes_pending)
struct reassembler {
    struct bytestream *output;  // Stream where we write reassembled data
    uint16_t next_seqno;       // Next sequence number we expect to receive
    
    // Array of slots for out-of-order segments
    // For example, if we receive segments in order: 3,1,4,2
    // We would:
    // 1. Store segment 3 in a slot, can't process yet (waiting for 1,2)
    // 2. Store segment 1 in a slot, process it immediately
    // 3. Store segment 4 in a slot, can't process yet (waiting for 2)
    // 4. Store segment 2 in a slot, then process 2,3,4 in order
    struct pending_segment segments[MAX_PENDING_SEGMENTS];  
    
    size_t capacity;           // Maximum total bytes we can store in slots
    size_t bytes_pending;      // Current total bytes stored in slots
};

// Create a new reassembler that will write its reassembled output to the given bytestream.
// The capacity limits how many total bytes can be stored in pending segments.
struct reassembler* reassembler_init(struct bytestream *output, size_t capacity);

// Try to insert a new segment into the reassembler:
// 1. If the segment's seqno matches next_seqno, write it to output immediately
// 2. If the segment's seqno is higher, store it in a slot until we can process it
// 3. If the segment's seqno is lower, it's a duplicate - ignore it
// Returns number of bytes successfully inserted
size_t reassembler_insert(struct reassembler *r, const uint8_t *data, 
                         size_t len, uint16_t seqno, bool is_last);

// Get the next sequence number we're expecting
// This is used by TCP to know what to ACK
uint16_t reassembler_next_seqno(const struct reassembler *r);

// Get number of bytes currently stored in slots waiting to be processed
size_t reassembler_bytes_pending(const struct reassembler *r);

// Check if we've received and processed all segments
// (no gaps in the sequence numbers)
bool reassembler_is_complete(const struct reassembler *r);