#pragma once

#include "rcp-datagram.h"

// Forward declarations
typedef struct tcp_peer tcp_peer_t;
typedef struct sender sender_t;
typedef struct receiver receiver_t;

typedef struct receiver_segment {
    uint16_t ackno;        // Sequence number of the ACK
    bool is_ack;           // Whether the segment is an ACK
    uint16_t window_size;  // Advertised window size
} receiver_segment_t;

typedef struct sender_segment {
    uint16_t seqno;
    bool is_syn;  // Whether the segment is a SYN
    bool is_fin;  // Whether the segment is a FIN
    size_t len;   // Length of the payload
    uint8_t payload[RCP_MAX_PAYLOAD];
} sender_segment_t;

sender_segment_t rcp_to_sender_segment(rcp_datagram_t *datagram) {
    sender_segment_t seg = {
        .seqno = datagram->header.seqno,
        .is_syn = rcp_has_flag(&datagram->header, RCP_FLAG_SYN),
        .is_fin = rcp_has_flag(&datagram->header, RCP_FLAG_FIN),
        .len = datagram->header.payload_len,
    };
    memcpy(seg.payload, datagram->payload, seg.len);
    return seg;
}

receiver_segment_t rcp_to_receiver_segment(rcp_datagram_t *datagram) {
    receiver_segment_t seg = {
        .ackno = datagram->header.ackno,
        .is_ack = rcp_has_flag(&datagram->header, RCP_FLAG_ACK),
        .window_size = datagram->header.window,
    };
    return seg;
}