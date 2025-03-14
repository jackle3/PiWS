#pragma once

#include "nrf.h"
#include "receiver.h"
#include "sender.h"

// TCP connection state
enum tcp_state
{
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT,
    TCP_CLOSED_WAIT,
    TCP_CLOSING,
    TCP_TIME_WAIT,
};

// TCP connection structure
struct tcp_connection
{
    struct sender *sender;     // Sender for outgoing data
    struct receiver *receiver; // Receiver for incoming data
    nrf_t *nrf;                // NRF radio interface
    uint32_t remote_addr;      // Remote address
    enum tcp_state state;      // Current connection state
    bool is_server;            // Whether this is server or client (client sends SYN first)
    uint32_t last_time;        // Last activity timestamp
};

// Initialize TCP connection
struct tcp_connection *tcp_init(nrf_t *nrf, uint32_t remote_addr, bool is_server);

// Send data
int tcp_send(struct tcp_connection *tcp, const void *data, size_t len);

// Receive data
int tcp_recv(struct tcp_connection *tcp, void *data, size_t len);

// Close connection
void tcp_close(struct tcp_connection *tcp);

// Handle handshake
int tcp_do_handshake(struct tcp_connection *tcp);

// Send segment
int tcp_send_segment(struct tcp_connection *tcp, const struct unacked_segment *seg);

// Receive packet
int tcp_recv_packet(struct tcp_connection *tcp, struct rcp_datagram *dgram);

// Send ACK
int tcp_send_ack(struct tcp_connection *tcp, const struct rcp_header *ack);

// Check and retransmit any expired segments
// - current_time_us should be from timer_get_usec()
// Returns number of segments retransmitted, or -1 on error
int tcp_check_retransmit(struct tcp_connection *tcp, uint32_t current_time_us);

uint8_t nrf_to_rcp_addr(uint32_t nrf_addr);
uint32_t rcp_to_nrf_addr(uint8_t rcp_addr);