#pragma once

#include "rcp-datagram.h"
#include "tcp.h"

/* Forward declarations for functions */
static inline sender_segment_t rcp_to_sender_segment(rcp_datagram_t *datagram);
static inline receiver_segment_t rcp_to_receiver_segment(rcp_datagram_t *datagram);
static inline rcp_datagram_t sender_segment_to_rcp(tcp_peer_t *peer, sender_segment_t *segment);
static inline rcp_datagram_t receiver_segment_to_rcp(tcp_peer_t *peer, receiver_segment_t *segment);

/**
 * Convert an RCP datagram to a sender segment
 *
 * @param datagram Pointer to the RCP datagram to convert
 * @return A sender segment structure containing the converted data
 */
static inline sender_segment_t rcp_to_sender_segment(rcp_datagram_t *datagram) {
    assert(datagram);

    sender_segment_t seg = {
        .seqno = datagram->header.seqno,
        .is_syn = rcp_has_flag(&datagram->header, RCP_FLAG_SYN),
        .is_fin = rcp_has_flag(&datagram->header, RCP_FLAG_FIN),
        .len = datagram->header.payload_len,
    };

    /* Copy payload if present */
    if (datagram->payload && datagram->header.payload_len > 0) {
        memcpy(seg.payload, datagram->payload, seg.len);
    }

    return seg;
}

/**
 * Convert an RCP datagram to a receiver segment
 *
 * @param datagram Pointer to the RCP datagram to convert
 * @return A receiver segment structure containing the converted data
 */
static inline receiver_segment_t rcp_to_receiver_segment(rcp_datagram_t *datagram) {
    assert(datagram);

    receiver_segment_t seg = {
        .ackno = datagram->header.ackno,
        .is_ack = rcp_has_flag(&datagram->header, RCP_FLAG_ACK),
        .window_size = datagram->header.window,
    };

    return seg;
}

/**
 * Convert a sender segment to an RCP datagram
 *
 * @param peer Pointer to the TCP peer containing addressing information
 * @param segment Pointer to the sender segment to convert
 * @return An RCP datagram containing the converted data
 */
static inline rcp_datagram_t sender_segment_to_rcp(tcp_peer_t *peer, sender_segment_t *segment) {
    assert(peer);
    assert(segment);

    rcp_datagram_t datagram = rcp_datagram_init();

    /* Set the source and destination addresses */
    datagram.header.src = peer->local_addr;
    datagram.header.dst = peer->remote_addr;

    /* Set the flags */
    if (segment->is_syn) {
        rcp_set_flag(&datagram.header, RCP_FLAG_SYN);
    }

    if (segment->is_fin) {
        rcp_set_flag(&datagram.header, RCP_FLAG_FIN);
    }

    /* Set the sequence number */
    datagram.header.seqno = segment->seqno;

    /* Set the payload (only if there is data to send) */
    if (segment->len > 0) {
        rcp_datagram_set_payload(&datagram, segment->payload, segment->len);
    }

    /* Zero out the unused fields (for the receiving message) */
    datagram.header.ackno = 0;
    datagram.header.window = 0;

    /* Compute the checksum over header and payload */
    rcp_datagram_compute_checksum(&datagram);

    return datagram;
}

/**
 * Convert a receiver segment to an RCP datagram
 *
 * @param peer Pointer to the TCP peer containing addressing information
 * @param segment Pointer to the receiver segment to convert
 * @return An RCP datagram containing the converted data
 */
static inline rcp_datagram_t receiver_segment_to_rcp(tcp_peer_t *peer,
                                                     receiver_segment_t *segment) {
    assert(peer);
    assert(segment);

    rcp_datagram_t datagram = rcp_datagram_init();

    /* Set the source and destination addresses */
    datagram.header.src = peer->local_addr;
    datagram.header.dst = peer->remote_addr;

    /* Set the ACK flag if needed */
    if (segment->is_ack) {
        rcp_set_flag(&datagram.header, RCP_FLAG_ACK);
    }

    /* Set the acknowledgment number */
    datagram.header.ackno = segment->ackno;

    /* Set the window size */
    datagram.header.window = segment->window_size;

    /* Zero out the unused fields (for the sending message) */
    datagram.header.seqno = 0;
    datagram.header.payload_len = 0;

    /* Compute the checksum over header only (no payload) */
    rcp_datagram_compute_checksum(&datagram);

    return datagram;
}