#pragma once

#include "receiver.h"
#include "sender.h"

// TCP peer
typedef struct tcp_peer {
    sender_t sender;
    receiver_t receiver;

    uint32_t local_addr;   // Local RCP address
    uint32_t remote_addr;  // Remote RCP address
} tcp_peer_t;

/**
 * Callback function for transmitting segments
 * @param peer The peer that will transmit the segment to its remote peer
 * @param segment The segment to transmit
 */
void transmit_segment(tcp_peer_t *peer, sender_segment_t *segment) {
    (void)peer;
    (void)segment;
}

/**
 * Callback function for transmitting ACK replies
 * @param peer The peer that will transmit the reply to its remote peer
 * @param segment The reply to transmit
 */
void transmit_reply(tcp_peer_t *peer, receiver_segment_t *segment) {
    (void)peer;
    (void)segment;
}

/**
 * Initialize a new TCP peer
 * @param nrf The NRF interface to use for sending and receiving segments
 * @param local_addr The local RCP address
 * @param remote_addr The remote RCP address
 */
tcp_peer_t tcp_peer_init(nrf_t *nrf, uint32_t local_addr, uint32_t remote_addr) {
    tcp_peer_t peer;
    peer.sender = sender_init(nrf, transmit_segment, &peer);
    peer.receiver = receiver_init(nrf, transmit_reply, &peer);
    return peer;
}
