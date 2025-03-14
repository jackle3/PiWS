/**
 * Bytestream structure for managing circular buffer of bytes
 *
 * The bytestream connects the TCP sender and receiver to their application interfaces.
 *
 * outgoing bytestream:
 * - The application writes data to the bytestream
 * - The sender reads data from the bytestream and sends it via NRF
 *
 * incoming bytestream:
 * - When the receiver receives a packet via NRF, the receiver writes the data to the bytestream
 * - The application reads data from the bytestream
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rpi.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define BS_CAPACITY 1024

struct bytestream {
    uint8_t buffer[BS_CAPACITY];  // Circular buffer
    size_t read_pos;              // Position for next read
    size_t write_pos;             // Position for next write
    size_t bytes_available;       // Number of bytes available to read
};

// Given a pointer to a bytestream, initialize it
void bs_init(struct bytestream *bs);

// Write data from the <data> buffer to the bytestream
// Returns number of bytes written
size_t bs_write(struct bytestream *bs, const uint8_t *data, size_t len);

// Read data from the bytestream and write it to the <data> buffer
// Also removes the data from the bytestream. Returns number of bytes read
size_t bs_read(struct bytestream *bs, uint8_t *data, size_t len);

// Peek at data in the bytestream without removing it
// Returns number of bytes peeked
size_t bs_peek(const struct bytestream *bs, uint8_t *data, size_t len);

// Get number of bytes available for reading
size_t bs_bytes_ready(const struct bytestream *bs);

// Get remaining capacity for writing data to the bytestream
size_t bs_remaining_capacity(const struct bytestream *bs);