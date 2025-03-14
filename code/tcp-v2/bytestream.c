#include "bytestream.h"

#include <string.h>

void bs_init(struct bytestream *bs) {
    memset(bs->buffer, 0, BS_CAPACITY);
    bs->read_pos = 0;
    bs->write_pos = 0;
    bs->bytes_available = 0;
}

size_t bs_write(struct bytestream *bs, const uint8_t *data, size_t len) {
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

    return bytes_to_write;
}

size_t bs_read(struct bytestream *bs, uint8_t *data, size_t len) {
    assert(bs);
    assert(data);

    // First peek the data
    size_t bytes_read = bs_peek(bs, data, len);

    // Update read position and bytes available
    bs->read_pos = (bs->read_pos + bytes_read) % BS_CAPACITY;
    bs->bytes_available -= bytes_read;

    return bytes_read;
}

size_t bs_peek(const struct bytestream *bs, uint8_t *data, size_t len) {
    assert(bs);
    assert(data);

    // Don't peek more than what's available
    size_t bytes_to_peek = MIN(len, bs_bytes_ready(bs));

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

size_t bs_bytes_ready(const struct bytestream *bs) {
    assert(bs);
    return bs->bytes_available;
}

size_t bs_remaining_capacity(const struct bytestream *bs) {
    assert(bs);
    return BS_CAPACITY - bs->bytes_available;
}