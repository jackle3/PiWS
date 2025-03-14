#include "bytestream.h"

#include <string.h>

struct bytestream *bytestream_init(size_t capacity) {
    // Allocate the bytestream struct
    struct bytestream *bs = kmalloc(sizeof(struct bytestream));
    if (!bs)
        return NULL;

    // Allocate the internal buffer
    bs->buffer = kmalloc(capacity);
    if (!bs->buffer) {
        return NULL;
    }

    // Initialize all fields
    bs->capacity = capacity;
    bs->read_pos = 0;
    bs->write_pos = 0;
    bs->bytes_available = 0;  // number of bytes available to read
    bs->_eof = false;

    return bs;
}

size_t bytestream_write(struct bytestream *bs, const uint8_t *data, size_t len) {
    // Validate inputs and check if stream is closed
    if (!bs || !data || bs->_eof)
        return 0;

    size_t bytes_to_write = len;
    size_t space_available = bs->capacity - bs->bytes_available;
    if (bytes_to_write > space_available) {
        bytes_to_write = space_available;
    }

    // Handle buffer wraparound - may need to split write into two parts
    size_t first_chunk = bs->capacity - bs->write_pos;
    if (bytes_to_write <= first_chunk) {
        // Can write all data in one chunk
        memcpy(bs->buffer + bs->write_pos, data, bytes_to_write);
    } else {
        // Need to split the write into two parts
        memcpy(bs->buffer + bs->write_pos, data, first_chunk);
        memcpy(bs->buffer, data + first_chunk, bytes_to_write - first_chunk);
    }

    // Update write position and bytes available to read
    bs->write_pos = (bs->write_pos + bytes_to_write) % bs->capacity;
    bs->bytes_available += bytes_to_write;

    return bytes_to_write;
}

size_t bytestream_read(struct bytestream *bs, uint8_t *data, size_t len) {
    if (!bs || !data)
        return 0;

    // First peek the data
    size_t bytes_read = bytestream_peek(bs, data, len);

    // Update read position and bytes available
    bs->read_pos = (bs->read_pos + bytes_read) % bs->capacity;
    bs->bytes_available -= bytes_read;

    return bytes_read;
}

size_t bytestream_peek(const struct bytestream *bs, uint8_t *data, size_t len) {
    if (!bs || !data)
        return 0;

    // Don't peek more than what's available
    size_t bytes_to_peek = len;
    if (bytes_to_peek > bs->bytes_available) {
        bytes_to_peek = bs->bytes_available;
    }

    // Handle buffer wraparound - may need to split peek into two parts
    size_t first_chunk = bs->capacity - bs->read_pos;
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

size_t bytestream_bytes_available(const struct bytestream *bs) {
    // Return number of bytes available to read, or 0 if stream is invalid
    return bs ? bs->bytes_available : 0;
}

size_t bytestream_remaining_capacity(const struct bytestream *bs) {
    // Return remaining write capacity, or 0 if stream is invalid
    return bs ? (bs->capacity - bs->bytes_available) : 0;
}

bool bytestream_eof(const struct bytestream *bs) {
    // Return true if stream is at EOF and no more data to read
    return bs ? (bs->_eof && bs->bytes_available == 0) : true;
}

void bytestream_end_input(struct bytestream *bs) {
    // Mark the stream as closed for writing
    if (bs)
        bs->_eof = true;
}