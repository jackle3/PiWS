#pragma once

#include "bytestream.h"
#include "nrf.h"
#include "queue-ext-T.h"
#include "types.h"

/* Forward declarations for segment types */
typedef struct tcp_peer tcp_peer_t;
typedef struct sender_segment sender_segment_t;
typedef struct receiver_segment receiver_segment_t;

/* Initial window size and timeout constants */
#define INITIAL_WINDOW_SIZE 1024
#define S_TO_US(s) ((s) * 1000000)
#define RTO_INITIAL_US S_TO_US(1)

/* Segments that have been sent but not yet acknowledged */
typedef struct unacked_segment {
    struct unacked_segment *next; /* Used for queue - next segment in the queue */
    sender_segment_t seg;         /* The actual segment that was sent */
} unacked_segment_t;

/* Retransmission queue */
typedef struct rtq {
    unacked_segment_t *head, *tail;
} rtq_t;

/* Generate queue functions for the retransmission queue */
gen_queue_T(rtq, rtq_t, head, tail, unacked_segment_t, next);

/* Function pointer type for transmitting segments to receiver */
typedef void (*sender_transmit_fn_t)(tcp_peer_t *peer, sender_segment_t *segment);

/* Sender state structure */
typedef struct sender {
    nrf_t *nrf;          /* Sender's NRF interface for sending segments */
    bytestream_t reader; /* App writes data to it, sender reads from it */

    uint16_t next_seqno;  /* Next sequence number to send */
    uint16_t acked_seqno; /* Sequence number of the highest acked segment */
    uint16_t window_size; /* Receiver's advertised window size */

    rtq_t pending_segs;      /* Queue of segments that have been sent but not yet acked */
    uint32_t initial_RTO_us; /* Initial RTO (in microseconds) */
    uint32_t rto_time_us;    /* Time when earliest outstanding segment will be retransmitted */
    uint32_t
        n_retransmits; /* Number of times the earliest outstanding segment has been retransmitted */

    sender_transmit_fn_t transmit; /* Callback to send segments to the remote peer */
    tcp_peer_t *peer;              /* Pointer to the TCP peer containing this sender */
} sender_t;

/* Function forward declarations */
static inline sender_t sender_init(nrf_t *nrf, sender_transmit_fn_t transmit, tcp_peer_t *peer);
static inline sender_segment_t make_segment(sender_t *sender, size_t len);
static inline void sender_send_segment(sender_t *sender, sender_segment_t seg);
static inline void sender_push(sender_t *sender);
static inline void sender_process_reply(sender_t *sender, receiver_segment_t *reply);
static inline void sender_check_retransmits(sender_t *sender);

/* External functions needed */
extern uint32_t timer_get_usec(void);
extern void *kmalloc(size_t size);

/**
 * Initialize the sender with default state
 *
 * @param nrf NRF interface for sending data
 * @param transmit Function to transmit segments to the receiver
 * @param peer Pointer to the TCP peer containing this sender
 * @return Initialized sender structure
 */
static inline sender_t sender_init(nrf_t *nrf, sender_transmit_fn_t transmit, tcp_peer_t *peer) {
    sender_t sender = {
        .nrf = nrf,
        .reader = bs_init(),
        .next_seqno = 0,
        .acked_seqno = 0,
        .window_size = INITIAL_WINDOW_SIZE,
        .initial_RTO_us = RTO_INITIAL_US,
        .rto_time_us = 0,
        .n_retransmits = 0,
        .transmit = transmit,
        .peer = peer,
    };
    rtq_init(&sender.pending_segs);
    return sender;
}

/**
 * Create a segment to be sent
 *
 * @param sender The sender to create the segment for
 * @param len The maximum length of the data to send
 * @return The created segment
 */
static inline sender_segment_t make_segment(sender_t *sender, size_t len) {
    assert(sender);

    sender_segment_t seg = {
        .len = 0,
        .seqno = sender->next_seqno,
        .is_syn = (sender->next_seqno == 0),
        .is_fin = false,
    };

    // Determine how many bytes to send (limit by max payload and requested length)
    size_t bytes_to_send = MIN(RCP_MAX_PAYLOAD, len);
    if (bytes_to_send > 0) {
        // Read data from bytestream into segment payload
        seg.len = bs_read(&sender->reader, seg.payload, bytes_to_send);
    }

    // Check if this is the FIN segment
    seg.is_fin = bs_reader_finished(&sender->reader);

    return seg;
}

/**
 * Send a segment to the remote peer
 *
 * @param sender The sender to send the segment from
 * @param seg The segment to send
 */
static inline void sender_send_segment(sender_t *sender, sender_segment_t seg) {
    assert(sender);

    // Send the segment to the remote peer
    sender->transmit(sender->peer, &seg);

    // Only track segments with data, SYN, or FIN
    if (seg.len > 0 || seg.is_syn || seg.is_fin) {
        // Create a new unacked segment and copy the segment data
        unacked_segment_t *pending = kmalloc(sizeof(unacked_segment_t));
        if (!pending) {
            // Handle memory allocation failure
            return;
        }

        // Copy the segment data
        memcpy(&pending->seg, &seg, sizeof(sender_segment_t));
        pending->next = NULL;

        // Set retransmission timer if this is the first segment in the queue
        if (rtq_empty(&sender->pending_segs)) {
            sender->rto_time_us = timer_get_usec() + sender->initial_RTO_us;
        }

        // Add the segment to the unacked queue
        rtq_push(&sender->pending_segs, pending);

        // Update next sequence number
        sender->next_seqno += seg.len;

        // SYN and FIN take up one sequence number each
        if (seg.is_syn || seg.is_fin) {
            sender->next_seqno++;
        }
    }
}

/**
 * Push data from the bytestream to be sent to the remote peer
 *
 * @param sender The sender to push data from
 */
static inline void sender_push(sender_t *sender) {
    assert(sender);

    // If FIN has been sent, no more data can be pushed
    if (bs_reader_finished(&sender->reader) &&
        (sender->next_seqno > bs_bytes_popped(&sender->reader) + 1)) {
        // The seqno of FIN is `1 + bytes_popped`, so if next_seqno is greater, we've sent FIN
        return;
    }

    // Edge case: if receiver window is 0 and no outstanding segments, send probe segment
    if (sender->window_size == 0) {
        if (rtq_empty(&sender->pending_segs)) {
            // Send a zero-length segment to probe for window update
            sender_send_segment(sender, make_segment(sender, 0));
        }
        return;
    }

    // Check if receiver has enough space to receive more data
    uint32_t receiver_max_seqno = sender->acked_seqno + sender->window_size;
    if (receiver_max_seqno < sender->next_seqno) {
        // No space in receiver window
        return;
    }

    // Send data if available in the bytestream
    if (bs_bytes_available(&sender->reader)) {
        uint32_t remaining_space = receiver_max_seqno - sender->next_seqno;
        sender_send_segment(sender, make_segment(sender, remaining_space));
    }
}

/**
 * Process a reply from the receiver
 *
 * @param sender The sender to process the reply for
 * @param reply The reply segment from the receiver
 */
static inline void sender_process_reply(sender_t *sender, receiver_segment_t *reply) {
    assert(sender);
    assert(reply);

    if (reply->is_ack) {
        // Validate ACK number doesn't exceed what we've sent
        if (reply->ackno > sender->next_seqno) {
            return;
        }

        // Update highest acknowledged sequence number
        sender->acked_seqno = reply->ackno;

        // Process acknowledged segments
        bool new_data_acked = false;
        while (!rtq_empty(&sender->pending_segs)) {
            unacked_segment_t *seg = rtq_start(&sender->pending_segs);

            // Calculate the sequence number after this segment
            uint16_t seg_end_seqno = seg->seg.seqno + seg->seg.len;
            if (seg->seg.is_syn || seg->seg.is_fin) {
                seg_end_seqno++;
            }

            // If this segment is not fully acknowledged, stop
            if (reply->ackno < seg_end_seqno) {
                break;
            }

            // Remove fully acknowledged segment from queue
            rtq_pop(&sender->pending_segs);
            new_data_acked = true;
        }

        // Reset retransmission timer if new data was acknowledged
        if (new_data_acked) {
            if (!rtq_empty(&sender->pending_segs)) {
                sender->rto_time_us = timer_get_usec() + sender->initial_RTO_us;
            }
            sender->n_retransmits = 0;
        }
    }

    // Update window size from receiver
    sender->window_size = reply->window_size;
}

/**
 * Check if any segments need to be retransmitted
 *
 * @param sender The sender to check for retransmits
 */
static inline void sender_check_retransmits(sender_t *sender) {
    assert(sender);

    // Only check if there are pending segments and the timer has expired
    uint32_t now_us = timer_get_usec();
    int32_t time_since_rto = now_us - sender->rto_time_us;
    if (time_since_rto >= 0 && !rtq_empty(&sender->pending_segs)) {
        // Retransmit the oldest unacknowledged segment
        unacked_segment_t *seg = rtq_start(&sender->pending_segs);
        sender->transmit(sender->peer, &seg->seg);

        // Update retransmission timer - use exponential backoff if window is nonzero
        if (sender->window_size) {
            // Exponential backoff: double RTO for each retransmission
            sender->rto_time_us = now_us + (sender->initial_RTO_us * (1 << sender->n_retransmits));
            sender->n_retransmits++;
        } else {
            // If window is zero, use fixed RTO for persistent probing
            sender->rto_time_us = now_us + sender->initial_RTO_us;
        }
    }
}