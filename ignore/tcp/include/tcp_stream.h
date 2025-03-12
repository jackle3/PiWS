#pragma once

#include "tcp_common.h"
#include <stdbool.h>

/* Bytestream structure for reading/writing data */
struct tcp_stream {
    uint8_t *buffer;         /* Circular buffer for data */
    size_t capacity;        /* Total buffer capacity */
    size_t read_pos;        /* Current read position */
    size_t write_pos;       /* Current write position */
    size_t bytes_buffered;  /* Number of bytes currently in buffer */
    bool _error;           /* Error state */
    bool _eof;            /* End of stream indicator */
};

/* Initialize a new bytestream with given capacity */
struct tcp_stream *tcp_stream_init(size_t capacity);

/* Write data to the stream */
ssize_t tcp_stream_write(struct tcp_stream *stream, const void *data, size_t len);

/* Read data from the stream */
ssize_t tcp_stream_read(struct tcp_stream *stream, void *data, size_t len);

/* Get number of bytes available to read */
size_t tcp_stream_available(const struct tcp_stream *stream);

/* Get remaining buffer capacity */
size_t tcp_stream_remaining_capacity(const struct tcp_stream *stream);

/* Check if stream has reached EOF */
bool tcp_stream_eof(const struct tcp_stream *stream);

/* Set EOF on stream */
void tcp_stream_set_eof(struct tcp_stream *stream);

/* Check if stream is in error state */
bool tcp_stream_error(const struct tcp_stream *stream);

/* Set error state on stream */
void tcp_stream_set_error(struct tcp_stream *stream);

/* Free stream resources */
void tcp_stream_free(struct tcp_stream *stream); 