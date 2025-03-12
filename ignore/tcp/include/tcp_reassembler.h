#pragma once

#include "tcp_common.h"
#include "tcp_stream.h"

/* Reassembly buffer entry */
struct tcp_segment {
    tcp_seqno_t seqno;           /* Sequence number */
    uint8_t *data;              /* Segment data */
    size_t len;                 /* Length of data */
    struct tcp_segment *next;   /* Next segment in list */
};

/* Reassembler structure */
struct tcp_reassembler {
    struct tcp_stream *output;   /* Output stream */
    struct tcp_segment *head;    /* Head of segment list */
    tcp_seqno_t next_seqno;     /* Next expected sequence number */
    size_t bytes_pending;       /* Bytes in reassembly queue */
};

/* Initialize reassembler */
struct tcp_reassembler *tcp_reassembler_init(struct tcp_stream *output);

/* Push a new segment into the reassembler */
int tcp_reassembler_push(struct tcp_reassembler *reasm,
                        tcp_seqno_t seqno,
                        const void *data,
                        size_t len);

/* Get number of bytes pending reassembly */
size_t tcp_reassembler_pending(const struct tcp_reassembler *reasm);

/* Get next expected sequence number */
tcp_seqno_t tcp_reassembler_next_seqno(const struct tcp_reassembler *reasm);

/* Free reassembler resources */
void tcp_reassembler_free(struct tcp_reassembler *reasm); 