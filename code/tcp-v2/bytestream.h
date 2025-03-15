/**
 * Bytestream structure for managing circular buffer of bytes
 *
 * The bytestream connects the TCP sender and receiver to their application interfaces.
 *
 * sender's bytestream:
 * - The application writes data to the bytestream
 * - The sender reads data from the bytestream and sends it via NRF
 *
 * receiver's bytestream:
 * - When the receiver receives a packet via NRF, the receiver writes the data to the bytestream
 * - The application reads data from the bytestream
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpi.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_WINDOW_SIZE UINT16_MAX
#define BS_CAPACITY MAX_WINDOW_SIZE

typedef struct bytestream {
    uint8_t buffer[MAX_WINDOW_SIZE];  // Circular buffer
    size_t read_pos;                  // Position for next read
    size_t write_pos;                 // Position for next write
    size_t bytes_available;           // Number of bytes available to read
    bool eof;                         // Whether the stream is at EOF

    size_t bytes_written;  // Number of bytes written to the stream
} bytestream_t;

// Given a pointer to a bytestream, initialize it
bytestream_t bs_init() {
    bytestream_t bs;
    memset(bs.buffer, 0, BS_CAPACITY);
    bs.read_pos = 0;
    bs.write_pos = 0;
    bs.bytes_available = 0;
    bs.bytes_written = 0;
    bs.eof = false;
    return bs;
}

// Get number of bytes available for reading
size_t bs_bytes_available(const bytestream_t *bs) {
    assert(bs);
    return bs->bytes_available;
}

// Get remaining capacity for writing data to the bytestream
size_t bs_remaining_capacity(const bytestream_t *bs) {
    assert(bs);
    return BS_CAPACITY - bs->bytes_available;
}

// Peek at data in the bytestream without removing it
// Returns number of bytes peeked
size_t bs_peek(const bytestream_t *bs, uint8_t *data, size_t len) {
    assert(bs);
    assert(data);

    // Don't peek more than what's available
    size_t bytes_to_peek = MIN(len, bs_bytes_available(bs));

    // Handle buffer wraparound - may need to split peek into two parts
    size_t first_chunk = BS_CAPACITY - bs->read_pos;
    if (bytes_to_peek <= first_chunk) {
        // Can peek all data in one chunk
        memcpy(data, bs->buffer + bs->read_pos, bytes_to_peek);
    } else {
        // Need to split the peek into two parts
        memcpy(data, bs->buffer + bs->read_pos, first_chunk);
        memcpy(data + first_chunk, bs->buffer, bytes_to_peek - first_chunk);
    }

    return bytes_to_peek;
}

// Read data from the bytestream and write it to the <data> buffer
// Also removes the data from the bytestream. Returns number of bytes read
size_t bs_read(bytestream_t *bs, uint8_t *data, size_t len) {
    assert(bs);
    assert(data);

    // First peek the data
    size_t bytes_read = bs_peek(bs, data, len);

    // Update read position and bytes available
    bs->read_pos = (bs->read_pos + bytes_read) % BS_CAPACITY;
    bs->bytes_available -= bytes_read;

    return bytes_read;
}

// Write data from the <data> buffer to the bytestream
// Returns number of bytes written
size_t bs_write(bytestream_t *bs, const uint8_t *data, size_t len) {
    assert(bs);
    assert(data);

    // Don't write more than the available space in the buffer
    size_t bytes_to_write = MIN(len, bs_remaining_capacity(bs));

    // Handle buffer wraparound - may need to split write into two parts
    size_t first_chunk = BS_CAPACITY - bs->write_pos;
    if (bytes_to_write <= first_chunk) {
        // Can write all data in one chunk
        memcpy(bs->buffer + bs->write_pos, data, bytes_to_write);
    } else {
        // Need to split the write into two parts
        memcpy(bs->buffer + bs->write_pos, data, first_chunk);
        memcpy(bs->buffer, data + first_chunk, bytes_to_write - first_chunk);
    }

    // Update write position and bytes available to read
    bs->write_pos = (bs->write_pos + bytes_to_write) % BS_CAPACITY;
    bs->bytes_available += bytes_to_write;
    bs->bytes_written += bytes_to_write;

    return bytes_to_write;
}

// Get number of bytes written to the stream
size_t bs_bytes_written(const bytestream_t *bs) {
    assert(bs);
    return bs->bytes_written;
}

// Get number of bytes popped from the stream
size_t bs_bytes_popped(const bytestream_t *bs) {
    assert(bs);
    return bs->bytes_written - bs->bytes_available;
}

// Check if the reading stream is finished (i.e. sender has read all data)
bool bs_reader_finished(const bytestream_t *bs) {
    assert(bs);
    return bs->eof && bs->bytes_available == 0;
}

// Check if the writing stream is finished (i.e. receiver has written all data)
bool bs_writer_finished(const bytestream_t *bs) {
    assert(bs);
    return bs->eof;
}

// Mark the stream as finished
void bs_end_input(bytestream_t *bs) {
    assert(bs);
    bs->eof = true;
}