#include "tcp.h"

#include <string.h>

// Static mapping between NRF addresses and RCP addresses
#define RCP_SERVER_ADDR 0x2
#define RCP_CLIENT_ADDR 0x1

// Timeout values in microseconds
#define RETRANSMIT_TIMEOUT_US (3 * 1000000)  // 3s in microseconds

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
    trace("[%s] My NRF address: %x\n", is_server ? "SERVER" : "CLIENT", nrf->rxaddr);
    trace("[%s] Remote NRF address: %x\n", is_server ? "SERVER" : "CLIENT", remote_addr);

    // Convert NRF addresses to RCP addresses
    uint8_t my_rcp_addr = nrf_to_rcp_addr(nrf->rxaddr);
    uint8_t remote_rcp_addr = nrf_to_rcp_addr(remote_addr);

    trace("[%s] My RCP address: %x\n", is_server ? "SERVER" : "CLIENT", my_rcp_addr);
    trace("[%s] Remote RCP address: %x\n\n", is_server ? "SERVER" : "CLIENT", remote_rcp_addr);

    tcp->sender = sender_init(my_rcp_addr, remote_rcp_addr, 1000);
    tcp->receiver = receiver_init(my_rcp_addr, remote_rcp_addr);
    if (!tcp->sender || !tcp->receiver) {
        kfree(tcp);
        return NULL;
    }

    tcp->nrf = nrf;
    tcp->remote_addr = remote_addr;  // Store NRF address for NRF operations
    tcp->state = TCP_CLOSED;
    tcp->is_server = is_server;
    tcp->last_time = timer_get_usec();
    tcp->fin_time = 0;  // Initialize fin_time to 0

    return tcp;
}

int tcp_do_handshake(struct tcp_connection *tcp) {
    if (!tcp)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    int ret;

    // Initialize state if needed
    if (tcp->state == TCP_CLOSED) {
        trace("Initializing TCP %s state...\n", tcp->is_server ? "SERVER" : "CLIENT");
        trace("[%s] My NRF address: %x\n", tcp->is_server ? "SERVER" : "CLIENT", tcp->nrf->rxaddr);
        trace("[%s] Remote NRF address: %x\n", tcp->is_server ? "SERVER" : "CLIENT",
              tcp->remote_addr);

        if (tcp->is_server) {
            // Server waits for SYN
            trace("[SERVER] Moving to TCP_LISTEN state\n");
            tcp->state = TCP_LISTEN;
        } else {
            // Client sends initial SYN immediately
            trace("[CLIENT] Sending initial SYN and moving to TCP_SYN_SENT state\n");
            struct rcp_datagram syn = rcp_datagram_init();
            syn.header.src = tcp->sender->src_addr;      // RCP address space
            syn.header.dst = tcp->sender->dst_addr;      // RCP address space
            syn.header.seqno = tcp->sender->next_seqno;  // Initial sequence number
            rcp_set_flag(&syn.header, RCP_FLAG_SYN);
            rcp_compute_checksum(&syn.header);

            if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) >= 0) {
                trace("[CLIENT] Sending initial SYN with seq=%d to NRF addr %x...\n",
                      syn.header.seqno, tcp->remote_addr);
                nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                tcp->state = TCP_SYN_SENT;
            }
        }
        tcp->last_time = timer_get_usec();
        return 0;
    }

    // Handle each state
    switch (tcp->state) {
        case TCP_LISTEN: {
            // SERVER STATE: waiting for SYN
            trace("[SERVER] TCP_LISTEN: Waiting for SYN at NRF address %x\n", tcp->nrf->rxaddr);

            struct rcp_datagram rx = rcp_datagram_init();
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_SYN)) {
                    trace("[SERVER] TCP_LISTEN: Received SYN with seq=%d from %x\n",
                          rx.header.seqno, rx.header.src);

                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;

                    // Send SYN-ACK
                    struct rcp_datagram synack = rcp_datagram_init();
                    synack.header.src = tcp->sender->src_addr;
                    synack.header.dst = tcp->sender->dst_addr;
                    synack.header.seqno = tcp->sender->next_seqno;  // Initial sequence number
                    synack.header.ackno = rx.header.seqno + 1;      // Acknowledge the SYN
                    rcp_set_flag(&synack.header, RCP_FLAG_SYN);
                    rcp_set_flag(&synack.header, RCP_FLAG_ACK);
                    rcp_compute_checksum(&synack.header);

                    if (rcp_datagram_serialize(&synack, buffer, RCP_TOTAL_SIZE) >= 0) {
                        trace("[SERVER] TCP_LISTEN: Sending SYN-ACK and moving to TCP_SYN_RECEIVED state\n");
                        trace("[SERVER] Sending SYN-ACK with seq=%d, ack=%d to %x...\n",
                              synack.header.seqno, synack.header.ackno, tcp->remote_addr);
                        nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                        tcp->sender->next_seqno++;  // Increment after sending SYN-ACK
                        tcp->state = TCP_SYN_RECEIVED;
                        tcp->last_time = timer_get_usec();
                    }
                }
            }
            break;
        }

        case TCP_SYN_SENT: {
            // CLIENT STATE: resend SYN periodically and check for SYN-ACK
            if (timer_get_usec() - tcp->last_time > RETRANSMIT_TIMEOUT_US) {
                trace("[CLIENT] TCP_SYN_SENT: Retransmitting SYN due to timeout\n");
                struct rcp_datagram syn = rcp_datagram_init();
                syn.header.src = tcp->sender->src_addr;
                syn.header.dst = tcp->sender->dst_addr;
                syn.header.seqno = tcp->sender->next_seqno;  // Keep using initial sequence number
                rcp_set_flag(&syn.header, RCP_FLAG_SYN);
                rcp_compute_checksum(&syn.header);

                if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) >= 0) {
                    trace("[CLIENT] Resending SYN with seq=%d to %x...\n",
                          syn.header.seqno, tcp->remote_addr);
                    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                }
                tcp->last_time = timer_get_usec();
            }

            // Check for SYN-ACK response
            struct rcp_datagram rx = rcp_datagram_init();
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_SYN) &&
                    rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    trace("[CLIENT] TCP_SYN_SENT: Received SYN-ACK with seq=%d, ack=%d from %x\n",
                          rx.header.seqno, rx.header.ackno, rx.header.src);

                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;

                    // Send ACK
                    struct rcp_header ack = {0};
                    ack.src = tcp->sender->src_addr;
                    ack.dst = tcp->sender->dst_addr;
                    ack.seqno = tcp->sender->next_seqno++;  // Use and increment
                    ack.ackno = rx.header.seqno + 1;        // Acknowledge their SYN
                    rcp_set_flag(&ack, RCP_FLAG_ACK);

                    if (tcp_send_ack(tcp, &ack) == 0) {
                        trace("[CLIENT] TCP_SYN_SENT: Sent ACK, moving to TCP_ESTABLISHED state\n");
                        tcp->state = TCP_ESTABLISHED;
                    }
                }
            }
            break;
        }

        case TCP_SYN_RECEIVED: {
            // SERVER STATE: waiting for ACK, resend SYN-ACK periodically
            if (timer_get_usec() - tcp->last_time > RETRANSMIT_TIMEOUT_US) {
                trace("[SERVER] TCP_SYN_RECEIVED: Retransmitting SYN-ACK due to timeout\n");
                struct rcp_datagram synack = rcp_datagram_init();
                synack.header.src = tcp->sender->src_addr;
                synack.header.dst = tcp->sender->dst_addr;
                synack.header.seqno = tcp->sender->next_seqno - 1;       // Use same seqno as before
                synack.header.ackno = tcp->receiver->reasm->next_seqno;  // Use stored ackno
                rcp_set_flag(&synack.header, RCP_FLAG_SYN);
                rcp_set_flag(&synack.header, RCP_FLAG_ACK);
                rcp_compute_checksum(&synack.header);

                if (rcp_datagram_serialize(&synack, buffer, RCP_TOTAL_SIZE) >= 0) {
                    trace("[SERVER] Resending SYN-ACK with seq=%d, ack=%d to %x...\n",
                          synack.header.seqno, synack.header.ackno, tcp->remote_addr);
                    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                }
                tcp->last_time = timer_get_usec();
            }

            // Check for ACK response
            struct rcp_datagram rx = rcp_datagram_init();
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    trace("[SERVER] TCP_SYN_RECEIVED: Received ACK with seq=%d, ack=%d from %x\n",
                          rx.header.seqno, rx.header.ackno, rx.header.src);

                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    trace("[SERVER] TCP_SYN_RECEIVED: Moving to TCP_ESTABLISHED state\n");
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

    trace("[%s] Sending segment seq=%d to NRF addr %x...\n", tcp->is_server ? "SERVER" : "CLIENT",
          seg->seqno, tcp->remote_addr);
    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
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

    if (rcp_has_flag(&dgram->header, RCP_FLAG_ACK)) {
        trace("[%s] Received ACK for seq=%d from RCP addr %x\n",
              tcp->is_server ? "SERVER" : "CLIENT", dgram->header.ackno, dgram->header.src);
    } else {
        trace("[%s] Received segment seq=%d from RCP addr %x\n",
              tcp->is_server ? "SERVER" : "CLIENT", dgram->header.seqno, dgram->header.src);
    }

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

    trace("[%s] Sending ACK for seq=%d to NRF addr %x\n", tcp->is_server ? "SERVER" : "CLIENT",
          ack->ackno, tcp->remote_addr);
    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);

    return 0;
}

int tcp_check_retransmit(struct tcp_connection *tcp, uint32_t current_time_us) {
    if (!tcp || !tcp->sender)
        return -1;

    int segments_retransmitted = 0;

    // Check all segments for timeout
    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++) {
        struct unacked_segment *seg = &tcp->sender->segments[i];
        if (!seg->acked && seg->send_time > 0 &&
            (current_time_us - seg->send_time) >= RETRANSMIT_TIMEOUT_US) {
            trace("\n\t\t[%s] RETRANSMITTING expired segment seq=%d (last sent %uus ago)\n",
                  tcp->is_server ? "SERVER" : "CLIENT", seg->seqno,
                  current_time_us - seg->send_time);

            // Retransmit the segment
            if (tcp_send_segment(tcp, seg) == 0) {
                segments_retransmitted++;
                break;
            }
        }
    }

    return segments_retransmitted;
}

int tcp_send(struct tcp_connection *tcp, const void *data, size_t len) {
    if (!tcp)
        return -1;
    
    // Check if connection is in a state that allows sending
    if (tcp->state != TCP_ESTABLISHED && tcp->state != TCP_CLOSE_WAIT) {
        trace("[%s] Cannot send data in state %d\n", 
              tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
        
        if (tcp->state == TCP_CLOSED)
            return -1;  // Connection closed
        else if (tcp->state == TCP_SYN_SENT || tcp->state == TCP_SYN_RECEIVED)
            return -2;  // Connection not established yet
        else 
            return -3;  // Connection closing
    }

    // Write to sender's stream
    size_t written = bytestream_write(tcp->sender->outgoing, data, len);
    if (written != len)
        return -1;

    size_t bytes_acked = 0;

    // Keep sending until all data is acknowledged
    while (bytes_acked < written) {
        // Process any state changes that might have occurred
        tcp_do_closing(tcp);
        
        // If connection has closed during sending, return error
        if (tcp->state != TCP_ESTABLISHED && tcp->state != TCP_CLOSE_WAIT) {
            trace("[%s] Connection state changed to %d during send\n", 
                  tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
            return bytes_acked > 0 ? bytes_acked : -1;
        }
        
        // Fill window with new segments
        sender_fill_window(tcp->sender);

        // Get next segment to send (new segments only)
        const struct unacked_segment *seg = sender_next_segment(tcp->sender);
        if (seg && seg->send_time == 0) {  // Only send if it's a new segment
            // Send segment
            if (tcp_send_segment(tcp, seg) < 0) {
                continue;
            }

            trace("[%s] Sending new segment seq=%d (bytes_acked=%d/%d)\n",
                  tcp->is_server ? "SERVER" : "CLIENT", seg->seqno, bytes_acked, written);
        }

        // Try to receive ACK with short timeout
        struct rcp_datagram ack = rcp_datagram_init();
        if (tcp_recv_packet(tcp, &ack) == 0) {
            // Check if it's a FIN packet, which would change the connection state
            if (rcp_has_flag(&ack.header, RCP_FLAG_FIN)) {
                // Handle the FIN by processing it through the closing handler
                trace("[%s] Received FIN during send, handling connection closing\n", 
                      tcp->is_server ? "SERVER" : "CLIENT");
                tcp_do_closing(tcp);
                
                // If we've moved to CLOSE_WAIT, we can continue sending
                if (tcp->state != TCP_ESTABLISHED && tcp->state != TCP_CLOSE_WAIT) {
                    trace("[%s] Connection state changed to %d during send\n", 
                         tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
                    return bytes_acked > 0 ? bytes_acked : -1;
                }
            }
            
            // Process ACK if present
            if (rcp_has_flag(&ack.header, RCP_FLAG_ACK)) {
                // Process ACK
                int newly_acked = sender_process_ack(tcp->sender, &ack.header);
                if (newly_acked > 0) {
                    bytes_acked += newly_acked * RCP_MAX_PAYLOAD;
                    trace("[%s] Received ACK for %d segments (bytes_acked=%d/%d)\n",
                          tcp->is_server ? "SERVER" : "CLIENT", newly_acked, bytes_acked, written);
                }
            }
        }
        
        // Check for retransmission timeouts
        tcp_check_retransmit(tcp, timer_get_usec());
    }

    return written;
}

int tcp_recv(struct tcp_connection *tcp, void *data, size_t len) {
    if (!tcp)
        return -1;
    
    // Check if connection is in a state that allows receiving
    if (tcp->state != TCP_ESTABLISHED && 
        tcp->state != TCP_FIN_WAIT_1 && 
        tcp->state != TCP_FIN_WAIT_2) {
        trace("[%s] Cannot receive data in state %d\n", 
              tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
        
        if (tcp->state == TCP_CLOSED)
            return -1;  // Connection closed
        else if (tcp->state == TCP_SYN_SENT || tcp->state == TCP_SYN_RECEIVED)
            return -2;  // Connection not established yet
        else if (tcp->state == TCP_CLOSE_WAIT || tcp->state == TCP_LAST_ACK)
            return 0;   // Normal EOF (remote side has closed)
        else 
            return -3;  // Connection in an invalid state for receiving
    }

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

        // Process any state changes
        tcp_do_closing(tcp);
        
        // If connection has moved to a state where we can't receive, return what we've got
        if (tcp->state != TCP_ESTABLISHED && 
            tcp->state != TCP_FIN_WAIT_1 && 
            tcp->state != TCP_FIN_WAIT_2) {
            trace("[%s] Connection state changed to %d during receive\n", 
                  tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
            
            // If we moved to CLOSE_WAIT, this is a normal EOF
            if (tcp->state == TCP_CLOSE_WAIT || tcp->state == TCP_LAST_ACK) {
                return bytes_received;  // Return what we got (might be 0 = EOF)
            }
            
            return bytes_received > 0 ? bytes_received : -1;
        }

        // Try to receive more data
        struct rcp_datagram dgram = rcp_datagram_init();
        if (tcp_recv_packet(tcp, &dgram) == 0) {
            // Check if it's a FIN packet
            if (rcp_has_flag(&dgram.header, RCP_FLAG_FIN)) {
                // Handle the FIN properly
                trace("[%s] Received FIN during receive\n", 
                      tcp->is_server ? "SERVER" : "CLIENT");
                tcp_do_closing(tcp);
                
                // If we've moved to CLOSE_WAIT, return what we've received so far
                if (tcp->state == TCP_CLOSE_WAIT) {
                    trace("[%s] Connection moved to CLOSE_WAIT, returning %d bytes\n", 
                          tcp->is_server ? "SERVER" : "CLIENT", bytes_received);
                    return bytes_received;  // This might be 0, indicating EOF
                }
            }
            
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
                  tcp->is_server ? "SERVER" : "CLIENT", bytes_received);
            break;
        }
    }

    return bytes_received;
}

// Non-blocking helper to send a FIN packet
int tcp_send_fin(struct tcp_connection *tcp) {
    if (!tcp)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];

    struct rcp_datagram fin = rcp_datagram_init();
    fin.header.src = tcp->sender->src_addr;
    fin.header.dst = tcp->sender->dst_addr;
    fin.header.seqno = tcp->sender->next_seqno++;
    rcp_set_flag(&fin.header, RCP_FLAG_FIN);
    rcp_compute_checksum(&fin.header);

    if (rcp_datagram_serialize(&fin, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    trace("[%s] Sending FIN with seq=%d to NRF addr %x\n", 
          tcp->is_server ? "SERVER" : "CLIENT", fin.header.seqno, tcp->remote_addr);
    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
    
    return 0;
}

// Process the TCP closing sequence
int tcp_do_closing(struct tcp_connection *tcp) {
    if (!tcp)
        return -1;
    
    // If we're already closed, nothing to do
    if (tcp->state == TCP_CLOSED)
        return 0;
    
    // Only established connections or connections in closing states
    if (tcp->state != TCP_ESTABLISHED && 
        tcp->state != TCP_FIN_WAIT_1 && 
        tcp->state != TCP_FIN_WAIT_2 && 
        tcp->state != TCP_CLOSE_WAIT && 
        tcp->state != TCP_LAST_ACK && 
        tcp->state != TCP_CLOSING && 
        tcp->state != TCP_TIME_WAIT) {
        return 0;
    }
    
    // Define MSL (Maximum Segment Lifetime) in microseconds
    // RFC recommends 2 minutes, but we'll use a shorter value for testing
    #define MSL_TIMEOUT_US (30 * 1000000)  // 30 seconds
    
    struct rcp_datagram rx = rcp_datagram_init();
    uint32_t current_time = timer_get_usec();
    
    // Handle each state
    switch (tcp->state) {
        case TCP_CLOSE_WAIT:
            // We've received a FIN, application needs to call tcp_close()
            trace("[%s] TCP_CLOSE_WAIT: Waiting for application to close\n", 
                  tcp->is_server ? "SERVER" : "CLIENT");
            break;
            
        case TCP_FIN_WAIT_1:
            // We've sent a FIN, waiting for ACK or FIN+ACK
            if (current_time - tcp->last_time > RETRANSMIT_TIMEOUT_US) {
                trace("[%s] TCP_FIN_WAIT_1: Retransmitting FIN due to timeout\n", 
                      tcp->is_server ? "SERVER" : "CLIENT");
                tcp_send_fin(tcp);
                tcp->last_time = current_time;
            }
            
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_FIN) && 
                    rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    // Received FIN+ACK
                    trace("[%s] TCP_FIN_WAIT_1: Received FIN+ACK\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    // Send ACK for the FIN
                    struct rcp_header ack = {0};
                    ack.src = tcp->sender->src_addr;
                    ack.dst = tcp->sender->dst_addr;
                    ack.seqno = tcp->sender->next_seqno++;
                    ack.ackno = rx.header.seqno + 1;
                    rcp_set_flag(&ack, RCP_FLAG_ACK);
                    
                    if (tcp_send_ack(tcp, &ack) == 0) {
                        trace("[%s] TCP_FIN_WAIT_1: Moving to TCP_TIME_WAIT\n", 
                              tcp->is_server ? "SERVER" : "CLIENT");
                        tcp->state = TCP_TIME_WAIT;
                        tcp->fin_time = current_time;
                    }
                } else if (rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    // Received ACK for our FIN
                    trace("[%s] TCP_FIN_WAIT_1: Received ACK for FIN\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    tcp->state = TCP_FIN_WAIT_2;
                    tcp->last_time = current_time;
                } else if (rcp_has_flag(&rx.header, RCP_FLAG_FIN)) {
                    // Received FIN (simultaneous close)
                    trace("[%s] TCP_FIN_WAIT_1: Received FIN (simultaneous close)\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    // Send ACK for the FIN
                    struct rcp_header ack = {0};
                    ack.src = tcp->sender->src_addr;
                    ack.dst = tcp->sender->dst_addr;
                    ack.seqno = tcp->sender->next_seqno++;
                    ack.ackno = rx.header.seqno + 1;
                    rcp_set_flag(&ack, RCP_FLAG_ACK);
                    
                    if (tcp_send_ack(tcp, &ack) == 0) {
                        trace("[%s] TCP_FIN_WAIT_1: Moving to TCP_CLOSING\n", 
                              tcp->is_server ? "SERVER" : "CLIENT");
                        tcp->state = TCP_CLOSING;
                        tcp->last_time = current_time;
                    }
                }
            }
            break;
            
        case TCP_FIN_WAIT_2:
            // We've received ACK for our FIN, waiting for FIN from other side
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_FIN)) {
                    trace("[%s] TCP_FIN_WAIT_2: Received FIN\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    // Send ACK for the FIN
                    struct rcp_header ack = {0};
                    ack.src = tcp->sender->src_addr;
                    ack.dst = tcp->sender->dst_addr;
                    ack.seqno = tcp->sender->next_seqno++;
                    ack.ackno = rx.header.seqno + 1;
                    rcp_set_flag(&ack, RCP_FLAG_ACK);
                    
                    if (tcp_send_ack(tcp, &ack) == 0) {
                        trace("[%s] TCP_FIN_WAIT_2: Moving to TCP_TIME_WAIT\n", 
                              tcp->is_server ? "SERVER" : "CLIENT");
                        tcp->state = TCP_TIME_WAIT;
                        tcp->fin_time = current_time;
                    }
                }
            }
            break;
            
        case TCP_CLOSING:
            // We've sent and received FIN, waiting for ACK
            if (current_time - tcp->last_time > RETRANSMIT_TIMEOUT_US) {
                // Resend ACK for the FIN we received
                struct rcp_header ack = {0};
                ack.src = tcp->sender->src_addr;
                ack.dst = tcp->sender->dst_addr;
                ack.seqno = tcp->sender->next_seqno;
                ack.ackno = tcp->receiver->reasm->next_seqno;
                rcp_set_flag(&ack, RCP_FLAG_ACK);
                
                if (tcp_send_ack(tcp, &ack) == 0) {
                    trace("[%s] TCP_CLOSING: Resending ACK\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    tcp->last_time = current_time;
                }
            }
            
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    trace("[%s] TCP_CLOSING: Received ACK for FIN\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    tcp->state = TCP_TIME_WAIT;
                    tcp->fin_time = current_time;
                }
            }
            break;
            
        case TCP_LAST_ACK:
            // We've been passively closed and sent a FIN, waiting for ACK
            if (current_time - tcp->last_time > RETRANSMIT_TIMEOUT_US) {
                trace("[%s] TCP_LAST_ACK: Retransmitting FIN due to timeout\n", 
                      tcp->is_server ? "SERVER" : "CLIENT");
                tcp_send_fin(tcp);
                tcp->last_time = current_time;
            }
            
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_ACK)) {
                    trace("[%s] TCP_LAST_ACK: Received ACK for FIN, moving to TCP_CLOSED\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    tcp->state = TCP_CLOSED;
                }
            }
            break;
            
        case TCP_TIME_WAIT:
            // Waiting for 2*MSL before closing
            if (current_time - tcp->fin_time >= 2 * MSL_TIMEOUT_US) {
                trace("[%s] TCP_TIME_WAIT: 2*MSL timeout expired, moving to TCP_CLOSED\n", 
                      tcp->is_server ? "SERVER" : "CLIENT");
                tcp->state = TCP_CLOSED;
            }
            
            // If we receive a retransmitted FIN, send an ACK again
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_FIN)) {
                    trace("[%s] TCP_TIME_WAIT: Received retransmitted FIN, resending ACK\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    
                    struct rcp_header ack = {0};
                    ack.src = tcp->sender->src_addr;
                    ack.dst = tcp->sender->dst_addr;
                    ack.seqno = tcp->sender->next_seqno;
                    ack.ackno = tcp->receiver->reasm->next_seqno;
                    rcp_set_flag(&ack, RCP_FLAG_ACK);
                    
                    tcp_send_ack(tcp, &ack);
                    // Reset the time_wait timer
                    tcp->fin_time = current_time;
                }
            }
            break;
            
        case TCP_ESTABLISHED:
            // In established state, check for incoming FIN
            if (tcp_recv_packet(tcp, &rx) == 0) {
                if (rcp_has_flag(&rx.header, RCP_FLAG_FIN)) {
                    trace("[%s] TCP_ESTABLISHED: Received FIN, passive close\n", 
                          tcp->is_server ? "SERVER" : "CLIENT");
                    
                    // Update receiver's expected sequence number
                    tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;
                    
                    // Send ACK for the FIN
                    struct rcp_header ack = {0};
                    ack.src = tcp->sender->src_addr;
                    ack.dst = tcp->sender->dst_addr;
                    ack.seqno = tcp->sender->next_seqno++;
                    ack.ackno = rx.header.seqno + 1;
                    rcp_set_flag(&ack, RCP_FLAG_ACK);
                    
                    if (tcp_send_ack(tcp, &ack) == 0) {
                        trace("[%s] TCP_ESTABLISHED: Moving to TCP_CLOSE_WAIT\n", 
                              tcp->is_server ? "SERVER" : "CLIENT");
                        tcp->state = TCP_CLOSE_WAIT;
                        tcp->last_time = current_time;
                    }
                }
            }
            break;
    }
    
    return tcp->state == TCP_CLOSED ? 1 : 0;
}

// Main TCP processing function that handles both handshake and closing
int tcp_process(struct tcp_connection *tcp) {
    if (!tcp)
        return -1;
    
    // If not yet established, try handshake
    if (tcp->state == TCP_CLOSED || tcp->state == TCP_LISTEN || 
        tcp->state == TCP_SYN_SENT || tcp->state == TCP_SYN_RECEIVED) {
        return tcp_do_handshake(tcp);
    }
    
    // Handle established and closing states
    return tcp_do_closing(tcp);
}

void tcp_close(struct tcp_connection *tcp) {
    if (!tcp)
        return;
    
    uint32_t current_time = timer_get_usec();
    
    // Handle different states
    switch (tcp->state) {
        case TCP_ESTABLISHED:
            // Active close - send FIN and move to FIN_WAIT_1
            trace("[%s] TCP_ESTABLISHED: Active close, sending FIN\n", 
                  tcp->is_server ? "SERVER" : "CLIENT");
            if (tcp_send_fin(tcp) == 0) {
                tcp->state = TCP_FIN_WAIT_1;
                tcp->last_time = current_time;
            }
            break;
            
        case TCP_CLOSE_WAIT:
            // Passive close - we already received FIN, now send our FIN
            trace("[%s] TCP_CLOSE_WAIT: Passive close, sending FIN\n", 
                  tcp->is_server ? "SERVER" : "CLIENT");
            if (tcp_send_fin(tcp) == 0) {
                tcp->state = TCP_LAST_ACK;
                tcp->last_time = current_time;
            }
            break;
            
        case TCP_CLOSED:
        case TCP_LISTEN:
        case TCP_SYN_SENT:
        case TCP_SYN_RECEIVED:
            // Can close immediately
            trace("[%s] Closing connection from state %d\n", 
                  tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
            tcp->state = TCP_CLOSED;
            break;
            
        default:
            // For other states, let the normal closing process continue
            trace("[%s] tcp_close() called in state %d, letting normal process continue\n", 
                  tcp->is_server ? "SERVER" : "CLIENT", tcp->state);
            break;
    }
}