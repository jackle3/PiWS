#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bytestream.h"
#include "rcp-datagram.h"
#include "reassembler.h"

#define RECEIVER_WINDOW_SIZE 8  // Maximum number of out-of-order segments
#define RECEIVER_BUFFER_SIZE (RECEIVER_WINDOW_SIZE * RCP_MAX_PAYLOAD)

// Receiver structure
struct receiver {
    struct bytestream *incoming;  // Incoming reassembled bytestream
    struct reassembler *reasm;    // Reassembler for out-of-order segments
    uint16_t window_size;         // Current window size
    uint8_t src_addr;             // Source (our) address
    uint8_t dst_addr;             // Destination (remote) address
};

// Initialize a new receiver
struct receiver *receiver_init(uint8_t src_addr, uint8_t dst_addr);

// Process a received segment
// Returns 0 on success, -1 on error
int receiver_process_segment(struct receiver *r, const struct rcp_datagram *dgram);

// Get acknowledgment information for the last processed segment
void receiver_get_ack(const struct receiver *r, struct rcp_header *ack);

// Get number of bytes available for reading
size_t receiver_bytes_available(const struct receiver *r);

// Read assembled data from the receiver
size_t receiver_read(struct receiver *r, uint8_t *data, size_t len);