#pragma once

#include "tcp_common.h"
#include "tcp_stream.h"
#include "tcp_network.h"
#include "rcp-datagram.h"

/* TCP Connection States */
enum tcp_state {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSING,
    TCP_TIME_WAIT,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK
};

/* TCP Connection structure */
struct tcp_connection {
    /* Connection identifiers */
    tcp_addr_t local_addr;
    tcp_addr_t remote_addr;
    tcp_port_t local_port;
    tcp_port_t remote_port;
    
    /* Connection state */
    enum tcp_state state;
    
    /* Sequence numbers */
    tcp_seqno_t send_next;    /* Next sequence number to send */
    tcp_seqno_t send_unack;   /* Oldest unacknowledged sequence number */
    tcp_seqno_t recv_next;    /* Next sequence number expected */
    
    /* Windows */
    uint16_t send_window;     /* Current send window size */
    uint16_t recv_window;     /* Current receive window size */
    
    /* Streams */
    struct tcp_stream *send_stream;    /* Outgoing data stream */
    struct tcp_stream *recv_stream;    /* Incoming data stream */
    
    /* Network interface */
    struct tcp_network *network;
    
    /* Retransmission */
    uint32_t rto;            /* Retransmission timeout (ms) */
    uint32_t srtt;           /* Smoothed round-trip time */
    uint32_t rttvar;         /* Round-trip time variation */
};

/* Initialize a new TCP connection */
struct tcp_connection *tcp_connection_init(struct tcp_network *net,
                                         tcp_addr_t local_addr,
                                         tcp_port_t local_port);

/* Connect to remote endpoint */
int tcp_connect(struct tcp_connection *conn,
               tcp_addr_t remote_addr,
               tcp_port_t remote_port);

/* Accept an incoming connection (blocks until connection received) */
int tcp_accept(struct tcp_connection *conn);

/* Send data on connection */
ssize_t tcp_send(struct tcp_connection *conn,
                 const void *data,
                 size_t len);

/* Receive data from connection */
ssize_t tcp_recv(struct tcp_connection *conn,
                 void *data,
                 size_t len);

/* Close connection */
int tcp_close(struct tcp_connection *conn);

/* Free connection resources */
void tcp_connection_free(struct tcp_connection *conn); 