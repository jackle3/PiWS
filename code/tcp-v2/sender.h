#pragma once

#include "bytestream.h"
#include "nrf.h"
#include "queue-ext-T.h"
#include "segments.h"

#define INITIAL_WINDOW_SIZE 32

#define S_TO_US(s) ((s) * 1000000)
#define RTO_INITIAL_US S_TO_US(1)

// Segments that have been sent but not yet acked
typedef struct unacked_segment {
    struct unacked_segment *next;  // Used for queue - next segment in the queue
    sender_segment_t seg;
} unacked_segment_t;

// Retransmission queue
typedef struct rtq {
    unacked_segment_t *head, *tail;
} rtq_t;

gen_queue_T(rtq, rtq_t, head, tail, unacked_segment_t, next);

typedef struct sender {
    nrf_t *nrf;           // Sender's NRF interface (to send segments)
    bytestream_t reader;  // App writes data to it, sender reads from it

    uint16_t next_seqno;   // Next sequence number to send
    uint16_t acked_seqno;  // Sequence number of the highest acked segment
    uint16_t window_size;  // Receiver's advertised window size

    rtq_t pending_segs;       // Queue of segments that have been sent but not yet acked
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
    sender.acked_seqno = 0;
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
    seg.len = 0;
    seg.seqno = sender->next_seqno;
    seg.is_syn = (seg.seqno == 0);

    size_t bytes_to_send = MIN(RCP_MAX_PAYLOAD, len);
    if (bytes_to_send > 0) {
        seg.len = bs_read(&sender->reader, seg.payload, bytes_to_send);
    }

    seg.is_fin = bs_finished(&sender->reader);

    return seg;
}

/**
 * Send a segment to the remote peer
 * @param sender The sender to send the segment for
 * @param seg The segment to send
 */
void sender_send_segment(sender_t *sender, sender_segment_t seg) {
    assert(sender);

    sender->transmit(&seg);

    // Add the message to the queue of outstanding segments
    if (seg.len > 0 || seg.is_syn || seg.is_fin) {
        unacked_segment_t *pending = kmalloc(sizeof(unacked_segment_t));
        // Copy the entire segment structure
        memcpy(&pending->seg, &seg, sizeof(sender_segment_t));
        pending->next = NULL;

        // Set retransmission timer if this is the first segment in the queue
        if (rtq_empty(&sender->pending_segs)) {
            sender->rto_time_us = timer_get_usec() + sender->initial_RTO_us;
        }

        // Add the segment to the queue
        rtq_push(&sender->pending_segs, pending);

        // Update next sequence number
        sender->next_seqno += seg.len;
        if (seg.is_syn || seg.is_fin)
            sender->next_seqno++;
    }
}

/**
 * After the user writes data to the bytestream, call this function to push the data to be
 * sent to the remote peer.
 *
 * @param sender The sender to push data to
 */
void sender_push(sender_t *sender) {
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
        if (rtq_empty(&sender->pending_segs)) {
            // Send a message to test the window size
            sender_send_segment(sender, make_segment(sender, 0));
        }
    }

    // If the receiver does not have enough space to receive the next seqno, do nothing
    uint16_t receiver_max_seqno = sender->acked_seqno + sender->window_size;
    if (receiver_max_seqno < sender->next_seqno) {
        return;
    }

    // Otherwise, send the segment if the bytestream has data to send
    if (bs_bytes_available(&sender->reader)) {
        uint16_t remaining_space = receiver_max_seqno - sender->next_seqno;
        sender_send_segment(sender, make_segment(sender, remaining_space));
    }
}

/**
 * Process a reply from the receiver. Reply might contain an ACK, or a window update.
 * @param sender The sender to process the reply for
 * @param reply The reply to process
 */
void sender_process_reply(sender_t *sender, receiver_segment_t *reply) {
    assert(sender);

    if (reply->is_ack) {
        // Invariant: the received ACK must not exceed maximum seqno we've sent
        if (reply->ackno > sender->next_seqno) {
            return;
        }

        sender->acked_seqno = reply->ackno;

        bool new_data = false;
        for (unacked_segment_t *seg = rtq_start(&sender->pending_segs); seg; seg = rtq_next(seg)) {
            // The queue is sorted, so stop when we find a message that is not fully acked
            if (reply->ackno < seg->seg.seqno + seg->seg.len) {
                break;
            }

            // If we reach here, outstanding segment is fully acked, remove it
            rtq_pop(&sender->pending_segs);
            new_data = true;
        }

        // If we received new data, reset the RTO
        if (new_data) {
            sender->rto_time_us = timer_get_usec() + sender->initial_RTO_us;
            sender->n_retransmits = 0;
        }
    }

    sender->window_size = reply->window_size;
}

/**
 * Check if any retransmits are needed. If so, retransmit the earliest outstanding segment.
 * @param sender The sender to check for retransmits
 */
void sender_check_retransmits(sender_t *sender) {
    assert(sender);

    uint32_t now_us = timer_get_usec();
    if (now_us >= sender->rto_time_us && !rtq_empty(&sender->pending_segs)) {
        // Retransmit the earliest outstanding segment
        unacked_segment_t *seg = rtq_start(&sender->pending_segs);
        sender->transmit(&seg->seg);  // Pass the sender_segment_t directly

        // If window size is nonzero, double RTO and update counter. Otherwise, reset RTO.
        if (sender->window_size) {
            sender->rto_time_us = now_us + sender->initial_RTO_us * (2 << sender->n_retransmits);
            sender->n_retransmits++;
        } else {
            sender->rto_time_us = now_us + sender->initial_RTO_us;
        }
    }
}