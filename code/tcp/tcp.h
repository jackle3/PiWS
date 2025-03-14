#pragma once

#include "nrf.h"
#include "receiver.h"
#include "sender.h"

// Static mapping between NRF addresses and RCP addresses
#define RCP_ADDR 0x1
#define RCP_ADDR_2 0x2

// Time to wait in TIME_WAIT state (in microseconds)
#define TCP_TIME_WAIT_US (2 * 1000000)  // 2 seconds MSL (Maximum Segment Lifetime)

// Timeout values in microseconds
#define RETRANSMIT_TIMEOUT_US (50 * 1000)  // 50ms in microseconds

// TCP connection state
enum tcp_state {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,  // Active close: Sent FIN, waiting for ACK and FIN
    TCP_FIN_WAIT_2,  // Active close: Received ACK for FIN, waiting for FIN
    TCP_CLOSE_WAIT,  // Passive close: Received FIN, sent ACK, application not closed yet
    TCP_LAST_ACK,    // Passive close: Received FIN, sent ACK, sent FIN, waiting for ACK
    TCP_CLOSING,     // Active close: Sent FIN, received FIN but not ACK, waiting for ACK
    TCP_TIME_WAIT    // Either close: Received FIN, sent ACK, waiting for timeout
};

// TCP connection structure
struct tcp_connection {
    struct sender *sender;      // Sender for outgoing data
    struct receiver *receiver;  // Receiver for incoming data
    nrf_t *nrf;                 // NRF radio interface
    uint32_t remote_addr;       // Remote address
    enum tcp_state state;       // Current connection state
    bool is_server;             // Whether this is server or client (client sends SYN first)
    uint32_t last_time;         // Last activity timestamp
};

// Initialize TCP connection
struct tcp_connection *tcp_init(nrf_t *nrf, uint32_t remote_addr, bool is_server);

// Send data
int tcp_send(struct tcp_connection *tcp, const void *data, size_t len);

// Receive data
int tcp_recv(struct tcp_connection *tcp, void *data, size_t len);

// Close connection
void tcp_close(struct tcp_connection *tcp);

// Server-side handshake (receives SYN)
int tcp_server_handshake(struct tcp_connection *tcp);

// Client-side handshake (sends SYN)
int tcp_client_handshake(struct tcp_connection *tcp);

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

// Process connection closing steps (both active and passive)
// Returns 1 if fully closed, 0 if in progress, -1 on error
int tcp_process_closing(struct tcp_connection *tcp);

// Send a FIN packet to initiate closing
int tcp_send_fin(struct tcp_connection *tcp);

uint8_t nrf_to_rcp_addr(uint32_t nrf_addr);
uint32_t rcp_to_nrf_addr(uint8_t rcp_addr);