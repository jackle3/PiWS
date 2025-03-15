#pragma once

#include "bytestream.h"
#include "nrf.h"
#include "queue-ext-T.h"
#include "rcp-datagram.h"

#define INITIAL_WINDOW_SIZE 1024

#define S_TO_US(s) ((s) * 1000000)
#define RTO_INITIAL_US S_TO_US(1)

// Segments that have been sent but not yet acked
typedef struct unacked_segment {
    struct unacked_segment *next;  // Used for queue - next segment in the queue

    uint16_t seqno;      // Sequence number of the segment
    uint32_t send_time;  // Time when the segment was sent
    uint32_t rto;        // Retransmission timeout (updated with backoff)

    size_t len;  // Length of the payload
    uint8_t payload[RCP_MAX_PAYLOAD];
} unacked_segment_t;

typedef struct sender_segment {
    uint16_t seqno;
    bool is_syn;  // Whether the segment is a SYN
    bool is_fin;  // Whether the segment is a FIN
    size_t len;   // Length of the payload
    uint8_t payload[RCP_MAX_PAYLOAD];
} sender_segment_t;

// Retransmission queue
typedef struct rtq {
    unacked_segment_t *head, *tail;
} rtq_t;

gen_queue_T(rtq, rtq_t, head, tail, unacked_segment_t, next);

typedef struct sender {
    nrf_t *nrf;           // Sender's NRF interface (to send segments)
    bytestream_t reader;  // App writes data to it, sender reads from it

    uint16_t next_seqno;   // Next sequence number to send
    uint16_t window_size;  // Receiver's advertised window size

    rtq_t outstanding_segs;   // Queue of segments that have been sent but not yet acked
    uint32_t initial_RTO_us;  // Initial RTO (in microseconds)
    uint32_t rto_time_us;     // Time when earliest outstanding segment will be retransmitted
    uint32_t n_retransmits;   // # of times the earliest outstanding segment has been retransmitted

    void (*transmit)(sender_segment_t *segment);  // Callback to send segments to the remote peer
} sender_t;

/**
 * Initialize the sender with default state
 */
sender_t sender_init(nrf_t *nrf, void (*transmit)(sender_segment_t *segment)) {
    sender_t sender;
    sender.nrf = nrf;
    sender.reader = bs_init();
    sender.next_seqno = 0;
    sender.window_size = INITIAL_WINDOW_SIZE;

    sender.initial_RTO_us = RTO_INITIAL_US;
    sender.rto_time_us = 0;
    sender.n_retransmits = 0;
    sender.transmit = transmit;
    return sender;
}

/**
 * Create a segment to be sent
 * @param sender The sender to create the segment for
 * @param len The length of the data to send
 * @return The created segment
 */
sender_segment_t make_segment(sender_t *sender, size_t len) {
    sender_segment_t seg;
    seg.seqno = sender->next_seqno;
    seg.is_syn = (seg.seqno == 0);
    seg.is_fin = bs_finished(&sender->reader);

    size_t bytes_to_send = MIN(RCP_MAX_PAYLOAD, len);
    if (bytes_to_send > 0) {
        bs_read(&sender->reader, seg.payload, bytes_to_send);
    }
    seg.len = bytes_to_send;

    return seg;
}

/**
 * After the user writes data to the bytestream, call this function to push the data to be
 * sent to the remote peer.
 *
 * @param sender The sender to push data to
 * @param remote_addr The remote RCP address of the receiver
 */
void sender_push(sender_t *sender, uint32_t remote_addr) {
    assert(sender);

    // Invariant: once FIN has been sent, no more data can be pushed
    if (bs_finished(&sender->reader) &&
        (sender->next_seqno > bs_bytes_popped(&sender->reader) + 1)) {
        // The seqno of FIN is `1 + bytes_popped`, so if the next seqno is greater, we've sent FIN
        return;
    }

    // Edge case: if the receiver window is 0, and we have no outstanding segments, send a message
    // to test the window size
    if (sender->window_size == 0) {
        if (rtq_empty(&sender->outstanding_segs)) {
            // Send a message to test the window size
            sender_segment_t seg = make_segment(sender, 0);
            sender->transmit(&seg);
        }
    }
}