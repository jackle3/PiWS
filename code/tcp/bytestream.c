#include "bytestream.h"
#include <string.h>

struct bytestream* bytestream_init(size_t capacity) {
    struct bytestream *bs = kmalloc(sizeof(struct bytestream));
    if (!bs) return NULL;

    bs->buffer = kmalloc(capacity);
    if (!bs->buffer) {
        return NULL;
    }

    bs->capacity = capacity;
    bs->read_pos = 0;
    bs->write_pos = 0;
    bs->bytes_available = 0;  // number of bytes available to read
    bs->_eof = false;

    return bs;
}

size_t bytestream_write(struct bytestream *bs, const uint8_t *data, size_t len) {
    if (!bs || !data || bs->_eof) return 0;

    size_t bytes_to_write = len;
    size_t space_available = bs->capacity - bs->bytes_available;
    // if (bytes_to_write > space_available) {
    //     bytes_to_write = space_available;
    // }

    // Only write if we can write all the data
    if (bytes_to_write > space_available) {
        return 0;
    }

    // Write data in two parts if wrapping around buffer end
    size_t first_chunk = bs->capacity - bs->write_pos;
    if (bytes_to_write <= first_chunk) {
        memcpy(bs->buffer + bs->write_pos, data, bytes_to_write);
    } else {
        memcpy(bs->buffer + bs->write_pos, data, first_chunk);
        memcpy(bs->buffer, data + first_chunk, bytes_to_write - first_chunk);
    }

    bs->write_pos = (bs->write_pos + bytes_to_write) % bs->capacity;
    bs->bytes_available += bytes_to_write;

    return bytes_to_write;
}

size_t bytestream_read(struct bytestream *bs, uint8_t *data, size_t len) {
    if (!bs || !data) return 0;

    size_t bytes_to_read = len;
    if (bytes_to_read > bs->bytes_available) {
        bytes_to_read = bs->bytes_available;
    }

    // Read data in two parts if wrapping around buffer end
    size_t first_chunk = bs->capacity - bs->read_pos;
    if (bytes_to_read <= first_chunk) {
        memcpy(data, bs->buffer + bs->read_pos, bytes_to_read);
    } else {
        memcpy(data, bs->buffer + bs->read_pos, first_chunk);
        memcpy(data + first_chunk, bs->buffer, bytes_to_read - first_chunk);
    }

    bs->read_pos = (bs->read_pos + bytes_to_read) % bs->capacity;
    bs->bytes_available -= bytes_to_read;

    return bytes_to_read;
}

size_t bytestream_peek(const struct bytestream *bs, uint8_t *data, size_t len) {
    if (!bs || !data) return 0;

    size_t bytes_to_peek = len;
    if (bytes_to_peek > bs->bytes_available) {
        bytes_to_peek = bs->bytes_available;
    }

    size_t first_chunk = bs->capacity - bs->read_pos;
    if (bytes_to_peek <= first_chunk) {
        memcpy(data, bs->buffer + bs->read_pos, bytes_to_peek);
    } else {
        memcpy(data, bs->buffer + bs->read_pos, first_chunk);
        memcpy(data + first_chunk, bs->buffer, bytes_to_peek - first_chunk);
    }

    return bytes_to_peek;
}

size_t bytestream_bytes_available(const struct bytestream *bs) {
    return bs ? bs->bytes_available : 0;
}

size_t bytestream_remaining_capacity(const struct bytestream *bs) {
    return bs ? (bs->capacity - bs->bytes_available) : 0;
}

bool bytestream_eof(const struct bytestream *bs) {
    return bs ? (bs->_eof && bs->bytes_available == 0) : true;
}

void bytestream_end_input(struct bytestream *bs) {
    if (bs) bs->_eof = true;
}