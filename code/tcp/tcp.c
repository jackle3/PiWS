#include "tcp.h"

#include <string.h>

// Static mapping between NRF addresses and RCP addresses
#define RCP_SERVER_ADDR 0x2
#define RCP_CLIENT_ADDR 0x1

// Helper function to get RCP address from NRF address
static uint8_t nrf_to_rcp_addr(uint32_t nrf_addr) {
    if (nrf_addr == server_addr)
        return RCP_SERVER_ADDR;
    if (nrf_addr == client_addr)
        return RCP_CLIENT_ADDR;
    return 0;  // Invalid address
}

// Helper function to get NRF address from RCP address
static uint32_t rcp_to_nrf_addr(uint8_t rcp_addr) {
    if (rcp_addr == RCP_SERVER_ADDR)
        return server_addr;
    if (rcp_addr == RCP_CLIENT_ADDR)
        return client_addr;
    return 0;  // Invalid address
}

struct tcp_connection *tcp_init(nrf_t *nrf, uint32_t remote_addr, bool is_server) {
    struct tcp_connection *tcp = kmalloc(sizeof(*tcp));
    if (!tcp)
        return NULL;

    trace("Initializing TCP connection...\n");
    trace("[%s] My NRF address: %x\n", is_server ? "server" : "client", nrf->rxaddr);
    trace("[%s] Remote NRF address: %x\n", is_server ? "server" : "client", remote_addr);
    
    // Convert NRF addresses to RCP addresses
    uint8_t my_rcp_addr = nrf_to_rcp_addr(nrf->rxaddr);
    uint8_t remote_rcp_addr = nrf_to_rcp_addr(remote_addr);
    
    trace("[%s] My RCP address: %x\n", is_server ? "server" : "client", my_rcp_addr);
    trace("[%s] Remote RCP address: %x\n\n", is_server ? "server" : "client", remote_rcp_addr);

    tcp->sender = sender_init(my_rcp_addr, remote_rcp_addr, 1000);
    tcp->receiver = receiver_init(my_rcp_addr, remote_rcp_addr);
    if (!tcp->sender || !tcp->receiver) {
        return NULL;
    }

    tcp->nrf = nrf;
    tcp->remote_addr = remote_addr;  // Store NRF address for NRF operations
    tcp->state = TCP_CLOSED;
    tcp->is_server = is_server;
    tcp->last_time = timer_get_usec();

    return tcp;
}

// int tcp_connect(struct tcp_connection *tcp) {
//     if (!tcp || tcp->state != TCP_CLOSED)
//         return -1;

//     // Send SYN
//     struct rcp_datagram syn = rcp_datagram_init();
//     syn.header.src = tcp->sender->src_addr;
//     syn.header.dst = tcp->remote_addr;
//     syn.header.seqno = tcp->sender->next_seqno++;
//     rcp_set_flag(&syn.header, RCP_FLAG_SYN);
//     rcp_compute_checksum(&syn.header);

//     uint8_t buffer[RCP_TOTAL_SIZE];
//     if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) < 0)
//         return -1;

//     trace("Sending SYN...\n");
//     nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
//     trace("SYN sent\n");
//     tcp->state = TCP_SYN_SENT;
//     return 0;
// }

// int tcp_accept(struct tcp_connection *tcp) {
//     if (!tcp)
//         return -1;

//     if (tcp->state == TCP_CLOSED) {
//         tcp->state = TCP_LISTEN;
//         return 0;
//     }
//     return -1;
// }

int tcp_do_handshake(struct tcp_connection *tcp) {
    if (!tcp)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    int ret;

    // Initialize state if needed
    if (tcp->state == TCP_CLOSED) {
        trace("Initializing state for %s...\n", tcp->is_server ? "server" : "client");
        trace("[%s] My NRF address: %x\n", tcp->is_server ? "server" : "client", tcp->nrf->rxaddr);
        trace("[%s] Remote NRF address: %x\n", tcp->is_server ? "server" : "client", tcp->remote_addr);
        
        if (tcp->is_server) {
            // Server waits for SYN
            tcp->state = TCP_LISTEN;
        } else {
            // Client sends initial SYN immediately
            struct rcp_datagram syn = rcp_datagram_init();
            syn.header.src = tcp->sender->src_addr;  // Already in RCP address space
            syn.header.dst = tcp->sender->dst_addr;  // Already in RCP address space
            syn.header.seqno = tcp->sender->next_seqno;  // Initial sequence number
            rcp_set_flag(&syn.header, RCP_FLAG_SYN);
            rcp_compute_checksum(&syn.header);

            if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) >= 0) {
                trace("[%s] Sending initial SYN with seq=%d to NRF addr %x...\n", 
                      tcp->is_server ? "server" : "client", syn.header.seqno, tcp->remote_addr);
                nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                tcp->state = TCP_SYN_SENT;
            }
        }
        return 0;
    }

    // Handle each state
    switch (tcp->state) {
        case TCP_LISTEN: {
            // Server: waiting for SYN
            trace("[%s] Waiting for SYN at NRF address %x\n", tcp->is_server ? "server" : "client", tcp->nrf->rxaddr);
            ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);  // Shorter timeout
            if (ret == RCP_TOTAL_SIZE) {
                struct rcp_datagram rx = rcp_datagram_init();
                if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                    return 0;

                if (rcp_has_flag(&rx.header, RCP_FLAG_SYN)) {
                    trace("[%s] Received SYN with seq=%d from %x\n", 
                          tcp->is_server ? "server" : "client", rx.header.seqno, rx.header.src);
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    // Send SYN-ACK
                    struct rcp_datagram synack = rcp_datagram_init();
                    synack.header.src = tcp->sender->src_addr;
                    synack.header.dst = tcp->sender->dst_addr;
                    synack.header.seqno = tcp->sender->next_seqno;  // Initial sequence number
                    synack.header.ackno = rx.header.seqno + 1;  // Acknowledge the SYN
                    rcp_set_flag(&synack.header, RCP_FLAG_SYN);
                    rcp_set_flag(&synack.header, RCP_FLAG_ACK);
                    rcp_compute_checksum(&synack.header);

                    if (rcp_datagram_serialize(&synack, buffer, RCP_TOTAL_SIZE) >= 0) {
                        trace("[%s] Sending SYN-ACK with seq=%d, ack=%d to %x...\n", 
                              tcp->is_server ? "server" : "client", synack.header.seqno, 
                              synack.header.ackno, tcp->remote_addr);
                        nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                        tcp->sender->next_seqno++;  // Increment after sending SYN-ACK
                        tcp->state = TCP_SYN_RECEIVED;
                    }
                }
            }
            break;
        }

        case TCP_SYN_SENT: {
            // Client: resend SYN periodically and check for SYN-ACK
            if (timer_get_usec() - tcp->last_time > 100000) {  // Resend every 100ms
                struct rcp_datagram syn = rcp_datagram_init();
                syn.header.src = tcp->sender->src_addr;
                syn.header.dst = tcp->sender->dst_addr;
                syn.header.seqno = tcp->sender->next_seqno;  // Keep using initial sequence number
                rcp_set_flag(&syn.header, RCP_FLAG_SYN);
                rcp_compute_checksum(&syn.header);

                if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) >= 0) {
                    trace("[%s] Resending SYN with seq=%d to %x...\n", 
                          tcp->is_server ? "server" : "client", syn.header.seqno, tcp->remote_addr);
                    nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                }
                tcp->last_time = timer_get_usec();
            }

            // Check for SYN-ACK response
            ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);  // Short timeout
            if (ret == RCP_TOTAL_SIZE) {
                struct rcp_datagram rx = rcp_datagram_init();
                if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                    return 0;

                if (rcp_has_flag(&rx.header, RCP_FLAG_SYN) &&
                    rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    trace("[%s] Received SYN-ACK with seq=%d, ack=%d from %x\n", 
                          tcp->is_server ? "server" : "client", rx.header.seqno, 
                          rx.header.ackno, rx.header.src);
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    // Update sender's sequence number
                    // tcp->sender->next_seqno++;  // Increment for the SYN we sent
                    
                    // Send ACK
                    struct rcp_datagram ack = rcp_datagram_init();
                    ack.header.src = tcp->sender->src_addr;
                    ack.header.dst = tcp->sender->dst_addr;
                    ack.header.seqno = tcp->sender->next_seqno++;  // Use and increment
                    ack.header.ackno = rx.header.seqno + 1;  // Acknowledge their SYN
                    rcp_set_flag(&ack.header, RCP_FLAG_ACK);
                    rcp_compute_checksum(&ack.header);

                    if (rcp_datagram_serialize(&ack, buffer, RCP_TOTAL_SIZE) >= 0) {
                        trace("[%s] Sending ACK with seq=%d, ack=%d to %x...\n", 
                              tcp->is_server ? "server" : "client", ack.header.seqno, 
                              ack.header.ackno, tcp->remote_addr);
                        nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                        tcp->state = TCP_ESTABLISHED;
                    }
                }
            }
            break;
        }

        case TCP_SYN_RECEIVED: {
            // Server: waiting for ACK, resend SYN-ACK periodically
            if (timer_get_usec() - tcp->last_time > 100000) {  // Resend every 100ms
                struct rcp_datagram synack = rcp_datagram_init();
                synack.header.src = tcp->sender->src_addr;
                synack.header.dst = tcp->sender->dst_addr;
                synack.header.seqno = tcp->sender->next_seqno - 1;  // Use same seqno as before
                synack.header.ackno = tcp->receiver->reasm->next_seqno;  // Use stored ackno
                rcp_set_flag(&synack.header, RCP_FLAG_SYN);
                rcp_set_flag(&synack.header, RCP_FLAG_ACK);
                rcp_compute_checksum(&synack.header);

                if (rcp_datagram_serialize(&synack, buffer, RCP_TOTAL_SIZE) >= 0) {
                    trace("[%s] Resending SYN-ACK with seq=%d, ack=%d to %x...\n", 
                          tcp->is_server ? "server" : "client", synack.header.seqno, 
                          synack.header.ackno, tcp->remote_addr);
                    nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                }
                tcp->last_time = timer_get_usec();
            }

            ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);  // Short timeout
            if (ret == RCP_TOTAL_SIZE) {
                struct rcp_datagram rx = rcp_datagram_init();
                if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                    return 0;

                if (rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    trace("[%s] Received ACK with seq=%d, ack=%d from %x\n", 
                          tcp->is_server ? "server" : "client", rx.header.seqno, 
                          rx.header.ackno, rx.header.src);
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    tcp->state = TCP_ESTABLISHED;
                }
            }
            break;
        }

        default:
            break;
    }

    return tcp->state == TCP_ESTABLISHED ? 1 : 0;
}

// Non-blocking helper to send a single segment
int tcp_send_segment(struct tcp_connection *tcp, const struct unacked_segment *seg) {
    if (!tcp || !seg)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    
    // Create RCP packet
    struct rcp_datagram dgram = rcp_datagram_init();
    dgram.header.src = tcp->sender->src_addr;  // Already in RCP address space
    dgram.header.dst = tcp->sender->dst_addr;  // Already in RCP address space
    dgram.header.seqno = seg->seqno;
    dgram.header.window = tcp->sender->window_size;

    if (rcp_datagram_set_payload(&dgram, seg->data, seg->len) < 0)
        return -1;

    rcp_compute_checksum(&dgram.header);

    // Serialize and send
    if (rcp_datagram_serialize(&dgram, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    trace("[%s] Sending segment seq=%d to NRF addr %x...\n", 
          tcp->is_server ? "server" : "client", seg->seqno, tcp->remote_addr);
    nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
    sender_segment_sent(tcp->sender, seg, timer_get_usec());

    return 0;
}

// Non-blocking helper to receive a single packet
int tcp_recv_packet(struct tcp_connection *tcp, struct rcp_datagram *dgram) {
    if (!tcp || !dgram)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    
    // Try to receive data with a short timeout
    int ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);
    if (ret != RCP_TOTAL_SIZE)
        return -1;

    if (rcp_datagram_parse(dgram, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    trace("[%s] Received segment seq=%d from RCP addr %x\n", 
          tcp->is_server ? "server" : "client", dgram->header.seqno, dgram->header.src);

    return 0;
}

// Non-blocking helper to send an ACK
int tcp_send_ack(struct tcp_connection *tcp, const struct rcp_header *ack) {
    if (!tcp || !ack)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    
    struct rcp_datagram ack_dgram = rcp_datagram_init();
    ack_dgram.header = *ack;
    rcp_compute_checksum(&ack_dgram.header);

    if (rcp_datagram_serialize(&ack_dgram, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    trace("[%s] Sending ACK for seq=%d to NRF addr %x\n", 
          tcp->is_server ? "server" : "client", ack->ackno, tcp->remote_addr);
    nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);

    return 0;
}

int tcp_send(struct tcp_connection *tcp, const void *data, size_t len) {
    if (!tcp || tcp->state != TCP_ESTABLISHED)
        return -1;

    // Write to sender's stream
    size_t written = bytestream_write(tcp->sender->outgoing, data, len);
    if (written != len)
        return -1;

    uint32_t curr_time = timer_get_usec();
    size_t bytes_acked = 0;

    // Keep sending until all data is acknowledged
    while (bytes_acked < written) {
        // Fill window with new segments
        sender_fill_window(tcp->sender);

        // Get next segment to send
        const struct unacked_segment *seg = sender_next_segment(tcp->sender);
        if (seg) {
            // Send segment
            if (tcp_send_segment(tcp, seg) < 0)
                continue;

            // Wait for ACK
            struct rcp_datagram ack = rcp_datagram_init();
            if (tcp_recv_packet(tcp, &ack) == 0 &&
                rcp_has_flag(&ack.header, RCP_FLAG_ACK)) {
                // Process ACK
                int newly_acked = sender_process_ack(tcp->sender, &ack.header);
                if (newly_acked > 0)
                    bytes_acked += newly_acked * RCP_MAX_PAYLOAD;
            }
        }

        // Check for retransmissions
        sender_check_retransmit(tcp->sender, curr_time);
        curr_time = timer_get_usec();
    }

    return written;
}

int tcp_recv(struct tcp_connection *tcp, void *data, size_t len) {
    if (!tcp || tcp->state != TCP_ESTABLISHED)
        return -1;

    size_t bytes_received = 0;
    uint32_t start_time = timer_get_usec();

    // Keep receiving until we get requested data or timeout
    while (bytes_received < len) {
        // Check if we already have enough data in the stream
        size_t available = bytestream_bytes_available(tcp->receiver->incoming);
        if (available > 0) {
            size_t to_read = len - bytes_received;
            if (to_read > available)
                to_read = available;

            size_t read =
                bytestream_read(tcp->receiver->incoming, (uint8_t *)data + bytes_received, to_read);
            bytes_received += read;
            if (bytes_received == len)
                break;
        }

        // Try to receive more data
        struct rcp_datagram dgram = rcp_datagram_init();
        if (tcp_recv_packet(tcp, &dgram) == 0) {
            // Process received segment
            if (receiver_process_segment(tcp->receiver, &dgram) == 0) {
                // Send ACK
                struct rcp_header ack = {0};
                receiver_get_ack(tcp->receiver, &ack);
                tcp_send_ack(tcp, &ack);
            }
        }

        // Check for timeout
        if (timer_get_usec() - start_time > 5000000) {  // 5 second timeout
            trace("[%s] Receive timeout after %d bytes\n", 
                  tcp->is_server ? "server" : "client", bytes_received);
            break;
        }
    }

    return bytes_received;
}

void tcp_close(struct tcp_connection *tcp) {
    if (!tcp || tcp->state != TCP_ESTABLISHED)
        return;

    // Send FIN
    struct rcp_datagram fin = rcp_datagram_init();
    fin.header.src = tcp->sender->src_addr;  // Already in RCP address space
    fin.header.dst = tcp->sender->dst_addr;  // Already in RCP address space
    fin.header.seqno = tcp->sender->next_seqno++;
    rcp_set_flag(&fin.header, RCP_FLAG_FIN);
    rcp_compute_checksum(&fin.header);

    uint8_t buffer[RCP_TOTAL_SIZE];
    if (rcp_datagram_serialize(&fin, buffer, RCP_TOTAL_SIZE) >= 0) {
        nrf_send_ack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
    }

    tcp->state = TCP_CLOSED;
}