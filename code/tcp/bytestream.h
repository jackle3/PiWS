#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rpi.h"

// Bytestream structure for managing circular buffer of bytes
struct bytestream {
    uint8_t *buffer;         // Circular buffer
    size_t capacity;         // Total buffer capacity
    size_t read_pos;         // Position for next read
    size_t write_pos;        // Position for next write
    size_t bytes_available;  // Number of bytes available to read
    bool _eof;               // End of stream indicator
};

// Initialize a new bytestream with given capacity
struct bytestream *bytestream_init(size_t capacity);

// Write data to the bytestream
// Returns number of bytes written
size_t bytestream_write(struct bytestream *bs, const uint8_t *data, size_t len);

// Read data from the bytestream and remove it from the buffer
// Returns number of bytes read
size_t bytestream_read(struct bytestream *bs, uint8_t *data, size_t len);

// Peek at data without removing it
// Returns number of bytes peeked
size_t bytestream_peek(const struct bytestream *bs, uint8_t *data, size_t len);

// Get number of bytes available for reading
size_t bytestream_bytes_available(const struct bytestream *bs);

// Get remaining capacity for writing
size_t bytestream_remaining_capacity(const struct bytestream *bs);

// Check if stream has reached EOF
bool bytestream_eof(const struct bytestream *bs);

// Mark stream as ended (no more writes allowed)
void bytestream_end_input(struct bytestream *bs);