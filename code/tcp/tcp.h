#pragma once

#include "nrf.h"
#include "receiver.h"
#include "sender.h"

// TCP connection state with client/server role indications
enum tcp_state {
    TCP_CLOSED,               // Both: Initial or closed state
    TCP_LISTEN,               // Server: Waiting for connection request
    TCP_SYN_SENT,             // Client: SYN sent, awaiting SYN+ACK
    TCP_SYN_RECEIVED,         // Server: SYN received, SYN+ACK sent, awaiting ACK
    TCP_ESTABLISHED,          // Both: Connection established

    // Connection termination states
    TCP_FIN_WAIT_1,           // Active close: Sent FIN, waiting for ACK or FIN+ACK
    TCP_FIN_WAIT_2,           // Active close: Received ACK for FIN, waiting for FIN
    TCP_CLOSE_WAIT,           // Passive close: Received FIN, sent ACK, waiting for application to close
    TCP_LAST_ACK,             // Passive close: Sent FIN, waiting for ACK
    TCP_CLOSING,              // Active close: Sent FIN, received FIN, waiting for ACK
    TCP_TIME_WAIT             // Active close: 2MSL wait after receiving FIN
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
    uint32_t fin_time;          // Timestamp when FIN was received (for TIME_WAIT)
};

// Initialize TCP connection
struct tcp_connection *tcp_init(nrf_t *nrf, uint32_t remote_addr, bool is_server);

// Send data
int tcp_send(struct tcp_connection *tcp, const void *data, size_t len);

// Receive data
int tcp_recv(struct tcp_connection *tcp, void *data, size_t len);

// Close connection
void tcp_close(struct tcp_connection *tcp);

// Process connection logic (handshake, data transfer, closing)
int tcp_process(struct tcp_connection *tcp);

// Handle handshake
int tcp_do_handshake(struct tcp_connection *tcp);

// Process closing sequence
int tcp_do_closing(struct tcp_connection *tcp);

// Send segment
int tcp_send_segment(struct tcp_connection *tcp, const struct unacked_segment *seg);

// Receive packet
int tcp_recv_packet(struct tcp_connection *tcp, struct rcp_datagram *dgram);

// Send ACK
int tcp_send_ack(struct tcp_connection *tcp, const struct rcp_header *ack);

// Send FIN
int tcp_send_fin(struct tcp_connection *tcp);

// Check and retransmit any expired segments
// - current_time_us should be from timer_get_usec()
// Returns number of segments retransmitted, or -1 on error
int tcp_check_retransmit(struct tcp_connection *tcp, uint32_t current_time_us);