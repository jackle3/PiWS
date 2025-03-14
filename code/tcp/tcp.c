#include "tcp.h"

#include <string.h>

// Helper function to get RCP address from NRF address
uint8_t nrf_to_rcp_addr(uint32_t nrf_addr)
{
    if (nrf_addr == server_addr || nrf_addr == client_addr)
        return RCP_ADDR;
    if (nrf_addr == server_addr_2 || nrf_addr == client_addr_2)
        return RCP_ADDR_2;
    return 0; // Invalid address
}

// Helper function to get NRF address from RCP address
uint32_t rcp_to_nrf_server_addr(uint8_t rcp_addr)
{
    if (rcp_addr == RCP_ADDR)
        return server_addr;
    if (rcp_addr == RCP_ADDR_2)
        return server_addr_2;
    return 0; // Invalid address
}

// same for client
uint32_t rcp_to_nrf_client_addr(uint8_t rcp_addr)
{
    if (rcp_addr == RCP_ADDR)
        return client_addr;
    if (rcp_addr == RCP_ADDR_2)
        return client_addr_2;
    return 0; // Invalid address
}

struct tcp_connection *tcp_init(nrf_t *nrf, uint8_t dst_rcp, bool is_server, uint32_t next_hop)
{
    struct tcp_connection *tcp = kmalloc(sizeof(*tcp));
    if (!tcp)
        return NULL;

    trace("Initializing TCP connection...\n");
    trace("[%s] My NRF address: %x\n", is_server ? "server" : "client", nrf->rxaddr);
    trace("[%s] Remote NRF address: %x\n", is_server ? "server" : "client", next_hop);

    // Convert NRF addresses to RCP addresses
    uint8_t my_rcp_addr = nrf_to_rcp_addr(nrf->rxaddr);

    trace("[%s] My RCP address: %x\n", is_server ? "server" : "client", my_rcp_addr);
    trace("[%s] Remote RCP address: %x\n\n", is_server ? "server" : "client", dst_rcp);

    tcp->sender = sender_init(my_rcp_addr, dst_rcp, 1000);
    tcp->receiver = receiver_init(my_rcp_addr, dst_rcp);
    if (!tcp->sender || !tcp->receiver)
    {
        return NULL;
    }

    tcp->nrf = nrf;
    tcp->remote_addr = next_hop; // Store NRF address for NRF operations
    tcp->state = TCP_CLOSED;
    tcp->is_server = is_server;
    tcp->last_time = timer_get_usec();

    return tcp;
}

// Server-side handshake (receives SYN)
int tcp_server_handshake(struct tcp_connection *tcp)
{
    if (!tcp)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    int ret;

    // Initialize state if needed
    if (tcp->state == TCP_CLOSED)
    {
        trace("Initializing state for server...\n");
        trace("[server] My NRF address: %x\n", tcp->nrf->rxaddr);
        trace("[server] Remote NRF address: %x\n", tcp->remote_addr);

        // Server waits for SYN
        tcp->state = TCP_LISTEN;
        return 0;
    }

    // Handle server states
    switch (tcp->state)
    {
    case TCP_LISTEN:
    {
        // Server: waiting for SYN
        trace("[server] Waiting for SYN at NRF address %x\n", tcp->nrf->rxaddr);
        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100); // Shorter timeout
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            if (rcp_has_flag(&rx.header, RCP_FLAG_SYN))
            {
                trace("[server] Received SYN with seq=%d from %x\n", rx.header.seqno,
                      rx.header.src);

                // Update receiver's expected sequence number
                tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;

                // Send SYN-ACK
                struct rcp_datagram synack = rcp_datagram_init();
                synack.header.src = tcp->sender->src_addr;
                synack.header.dst = tcp->sender->dst_addr;
                synack.header.seqno = tcp->sender->next_seqno; // Initial sequence number
                synack.header.ackno = rx.header.seqno + 1;     // Acknowledge the SYN
                rcp_set_flag(&synack.header, RCP_FLAG_SYN);
                rcp_set_flag(&synack.header, RCP_FLAG_ACK);
                rcp_compute_checksum(&synack.header);

                if (rcp_datagram_serialize(&synack, buffer, RCP_TOTAL_SIZE) >= 0)
                {
                    trace("[server] Sending SYN-ACK with seq=%d, ack=%d to %x...\n",
                          synack.header.seqno, synack.header.ackno, tcp->remote_addr);
                    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                    tcp->sender->next_seqno++; // Increment after sending SYN-ACK
                    tcp->state = TCP_SYN_RECEIVED;
                    tcp->last_time = timer_get_usec(); // Reset timer for retransmission
                }
            }
        }
        break;
    }

    case TCP_SYN_RECEIVED:
    {
        // Server: waiting for ACK, resend SYN-ACK periodically
        if (timer_get_usec() - tcp->last_time > RETRANSMIT_TIMEOUT_US)
        {
            struct rcp_datagram synack = rcp_datagram_init();
            synack.header.src = tcp->sender->src_addr;
            synack.header.dst = tcp->sender->dst_addr;
            synack.header.seqno = tcp->sender->next_seqno - 1;      // Use same seqno as before
            synack.header.ackno = tcp->receiver->reasm->next_seqno; // Use stored ackno
            rcp_set_flag(&synack.header, RCP_FLAG_SYN);
            rcp_set_flag(&synack.header, RCP_FLAG_ACK);
            rcp_compute_checksum(&synack.header);

            if (rcp_datagram_serialize(&synack, buffer, RCP_TOTAL_SIZE) >= 0)
            {
                trace("[server] Resending SYN-ACK with seq=%d, ack=%d to %x...\n",
                      synack.header.seqno, synack.header.ackno, tcp->remote_addr);
                nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
            }
            tcp->last_time = timer_get_usec();
        }

        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100); // Short timeout
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            if (rcp_has_flag(&rx.header, RCP_FLAG_ACK))
            {
                trace("[server] Received ACK with seq=%d, ack=%d from %x\n", rx.header.seqno,
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

// Client-side handshake (sends SYN)
int tcp_client_handshake(struct tcp_connection *tcp)
{
    if (!tcp)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    int ret;

    // Initialize state if needed
    if (tcp->state == TCP_CLOSED)
    {
        trace("Initializing state for client...\n");
        trace("[client] My NRF address: %x\n", tcp->nrf->rxaddr);
        trace("[client] Remote NRF address: %x\n", tcp->remote_addr);

        // Client sends initial SYN immediately
        struct rcp_datagram syn = rcp_datagram_init();
        syn.header.src = tcp->sender->src_addr;     // Already in RCP address space
        syn.header.dst = tcp->sender->dst_addr;     // Already in RCP address space
        syn.header.seqno = tcp->sender->next_seqno; // Initial sequence number
        rcp_set_flag(&syn.header, RCP_FLAG_SYN);
        rcp_compute_checksum(&syn.header);

        if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) >= 0)
        {
            trace("[client] Sending initial SYN with seq=%d to NRF addr %x...\n", syn.header.seqno,
                  tcp->remote_addr);
            nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
            tcp->state = TCP_SYN_SENT;
            tcp->last_time = timer_get_usec(); // Set initial time for retransmission
        }
        return 0;
    }

    // Handle client state
    if (tcp->state == TCP_SYN_SENT)
    {
        // Client: resend SYN periodically and check for SYN-ACK
        if (timer_get_usec() - tcp->last_time > RETRANSMIT_TIMEOUT_US)
        {
            struct rcp_datagram syn = rcp_datagram_init();
            syn.header.src = tcp->sender->src_addr;
            syn.header.dst = tcp->sender->dst_addr;
            syn.header.seqno = tcp->sender->next_seqno; // Keep using initial sequence number
            rcp_set_flag(&syn.header, RCP_FLAG_SYN);
            rcp_compute_checksum(&syn.header);

            if (rcp_datagram_serialize(&syn, buffer, RCP_TOTAL_SIZE) >= 0)
            {
                trace("[client] Resending SYN with seq=%d to %x...\n", syn.header.seqno,
                      tcp->remote_addr);
                nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
            }
            tcp->last_time = timer_get_usec();
        }

        // Check for SYN-ACK response
        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100); // Short timeout
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            if (rcp_has_flag(&rx.header, RCP_FLAG_SYN) && rcp_has_flag(&rx.header, RCP_FLAG_ACK))
            {
                trace("[client] Received SYN-ACK with seq=%d, ack=%d from %x\n", rx.header.seqno,
                      rx.header.ackno, rx.header.src);

                // Update receiver's expected sequence number
                tcp->receiver->reasm->next_seqno = rx.header.seqno + 1;

                // Send ACK
                struct rcp_datagram ack = rcp_datagram_init();
                ack.header.src = tcp->sender->src_addr;
                ack.header.dst = tcp->sender->dst_addr;
                ack.header.seqno = tcp->sender->next_seqno++; // Use and increment
                ack.header.ackno = rx.header.seqno + 1;       // Acknowledge their SYN
                rcp_set_flag(&ack.header, RCP_FLAG_ACK);
                rcp_compute_checksum(&ack.header);

                if (rcp_datagram_serialize(&ack, buffer, RCP_TOTAL_SIZE) >= 0)
                {
                    trace("[client] Sending ACK with seq=%d, ack=%d to %x...\n", ack.header.seqno,
                          ack.header.ackno, tcp->remote_addr);
                    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                    tcp->state = TCP_ESTABLISHED;
                }
            }
        }
    }

    return tcp->state == TCP_ESTABLISHED ? 1 : 0;
}

// Main handshake function that delegates to the appropriate handler
int tcp_do_handshake(struct tcp_connection *tcp)
{
    if (!tcp)
        return -1;

    // Delegate to the appropriate handshake function based on server/client role
    if (tcp->is_server)
    {
        return tcp_server_handshake(tcp);
    }
    else
    {
        return tcp_client_handshake(tcp);
    }
}

// Non-blocking helper to send a single segment
int tcp_send_segment(struct tcp_connection *tcp, const struct unacked_segment *seg)
{
    if (!tcp || !seg)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];

    // Create RCP packet
    struct rcp_datagram dgram = rcp_datagram_init();
    dgram.header.src = tcp->sender->src_addr; // Already in RCP address space
    dgram.header.dst = tcp->sender->dst_addr; // Already in RCP address space
    dgram.header.seqno = seg->seqno;
    dgram.header.window = tcp->sender->window_size;

    if (rcp_datagram_set_payload(&dgram, seg->data, seg->len) < 0)
        return -1;

    // Set FIN flag if this is a FIN segment
    if (seg->is_fin)
    {
        rcp_set_flag(&dgram.header, RCP_FLAG_FIN);
    }

    rcp_compute_checksum(&dgram.header);

    // Serialize and send
    if (rcp_datagram_serialize(&dgram, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    // trace("[%s] Sending segment seq=%d to NRF addr %x...\n", tcp->is_server ? "server" :
    // "client",
    //       seg->seqno, tcp->remote_addr);
    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
    sender_segment_sent(tcp->sender, seg, timer_get_usec());

    return 0;
}

// Non-blocking helper to receive a single packet
int tcp_recv_packet(struct tcp_connection *tcp, struct rcp_datagram *dgram)
{
    if (!tcp || !dgram)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];

    // Try to receive data with a short timeout
    int ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);
    if (ret != RCP_TOTAL_SIZE)
        return -1;

    if (rcp_datagram_parse(dgram, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    if (rcp_has_flag(&dgram->header, RCP_FLAG_ACK))
    {
        // trace("[%s] Received ACK for seq=%d from RCP addr %x\n",
        //       tcp->is_server ? "server" : "client", dgram->header.ackno, dgram->header.src);
    }
    else
    {
        // trace("[%s] Received segment seq=%d from RCP addr %x\n",
        //       tcp->is_server ? "server" : "client", dgram->header.seqno, dgram->header.src);
    }

    return 0;
}

// Non-blocking helper to send an ACK
int tcp_send_ack(struct tcp_connection *tcp, const struct rcp_header *ack)
{
    if (!tcp || !ack)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];

    struct rcp_datagram ack_dgram = rcp_datagram_init();
    ack_dgram.header = *ack;
    rcp_compute_checksum(&ack_dgram.header);

    if (rcp_datagram_serialize(&ack_dgram, buffer, RCP_TOTAL_SIZE) < 0)
        return -1;

    // trace("[%s] Sending ACK for seq=%d to NRF addr %x\n", tcp->is_server ? "server" : "client",
    //       ack->ackno, tcp->remote_addr);
    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);

    return 0;
}

int tcp_check_retransmit(struct tcp_connection *tcp, uint32_t current_time_us)
{
    if (!tcp || !tcp->sender)
        return -1;

    int segments_retransmitted = 0;

    // Check all segments for timeout
    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++)
    {
        struct unacked_segment *seg = &tcp->sender->segments[i];
        if (seg->acked)
            continue;
        if (seg->send_time == 0)
            continue;
        if ((current_time_us - seg->send_time) < RETRANSMIT_TIMEOUT_US)
            continue;

        // Retransmit the segment
        if (tcp_send_segment(tcp, seg) == 0)
        {
            segments_retransmitted++;
            break;
        }
    }

    return segments_retransmitted;
}

int tcp_send(struct tcp_connection *tcp, const void *data, size_t len)
{
    if (!tcp || tcp->state != TCP_ESTABLISHED)
        return -1;

    // Write to sender's stream
    size_t written = bytestream_write(tcp->sender->outgoing, data, len);
    if (written != len)
        return -1;

    size_t bytes_acked = 0;

    // Keep sending until all data is acknowledged
    while (bytes_acked < written)
    {
        // Fill window with new segments
        sender_fill_window(tcp->sender);

        // Get next segment to send (new segments only)
        const struct unacked_segment *seg = sender_next_segment(tcp->sender);
        if (seg && seg->send_time == 0)
        { // Only send if it's a new segment
            // Send segment
            if (tcp_send_segment(tcp, seg) < 0)
            {
                continue;
            }

            // trace("[%s] Sending new segment seq=%d (bytes_acked=%d/%d)\n",
            //       tcp->is_server ? "server" : "client", seg->seqno, bytes_acked, written);
        }

        // Try to receive ACK with short timeout
        struct rcp_datagram ack = rcp_datagram_init();
        if (tcp_recv_packet(tcp, &ack) == 0 && rcp_has_flag(&ack.header, RCP_FLAG_ACK))
        {
            // Process ACK
            int newly_acked = sender_process_ack(tcp->sender, &ack.header);
            if (newly_acked > 0)
            {
                bytes_acked += newly_acked * RCP_MAX_PAYLOAD;
                // trace("[%s] Received ACK for %d segments (bytes_acked=%d/%d)\n",
                //       tcp->is_server ? "server" : "client", newly_acked, bytes_acked, written);
            }
        }
    }

    return written;
}

int tcp_recv(struct tcp_connection *tcp, void *data, size_t len)
{
    if (!tcp || (tcp->state != TCP_ESTABLISHED && tcp->state != TCP_CLOSE_WAIT))
        return -1;

    size_t bytes_received = 0;
    uint32_t start_time = timer_get_usec();

    // Keep receiving until we get requested data or timeout
    while (bytes_received < len)
    {
        // Check if we already have enough data in the stream
        size_t available = bytestream_bytes_available(tcp->receiver->incoming);
        if (available > 0)
        {
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
        if (tcp_recv_packet(tcp, &dgram) == 0)
        {
            // Check for FIN packet
            if (rcp_has_flag(&dgram.header, RCP_FLAG_FIN) && tcp->state == TCP_ESTABLISHED)
            {
                trace("[%s] Received FIN, transitioning to CLOSE_WAIT\n",
                      tcp->is_server ? "server" : "client");

                // Send ACK for the FIN
                struct rcp_header ack = {0};
                ack.src = tcp->sender->src_addr;
                ack.dst = tcp->sender->dst_addr;
                ack.seqno = tcp->sender->next_seqno;
                ack.ackno = dgram.header.seqno + 1;
                rcp_set_flag(&ack, RCP_FLAG_ACK);
                rcp_compute_checksum(&ack);
                tcp_send_ack(tcp, &ack);

                // Transition to CLOSE_WAIT
                tcp->state = TCP_CLOSE_WAIT;
            }

            // Process received segment
            if (receiver_process_segment(tcp->receiver, &dgram) == 0)
            {
                // Send ACK
                struct rcp_header ack = {0};
                receiver_get_ack(tcp->receiver, &ack);
                tcp_send_ack(tcp, &ack);
            }
        }

        // Check for timeout
        if (timer_get_usec() - start_time > 5000000)
        { // 5 second timeout
            // trace("[%s] Receive timeout after %d bytes\n", tcp->is_server ? "server" : "client",
            //       bytes_received);
            break;
        }
    }

    return bytes_received;
}

void tcp_close(struct tcp_connection *tcp)
{
    if (!tcp)
        return;

    // Handle based on current state
    switch (tcp->state)
    {
    case TCP_ESTABLISHED:
        // Active close - send FIN and move to FIN_WAIT_1
        trace("[%s] Initiating active close from ESTABLISHED state\n",
              tcp->is_server ? "server" : "client");
        tcp_send_fin(tcp);
        tcp->state = TCP_FIN_WAIT_1;
        break;

    case TCP_CLOSE_WAIT:
        // Passive close - we already received FIN, now send our FIN
        trace("[%s] Completing passive close from CLOSE_WAIT state\n",
              tcp->is_server ? "server" : "client");
        tcp_send_fin(tcp);
        tcp->state = TCP_LAST_ACK;
        break;

    default:
        // For all other states, do nothing - closing is already in progress
        // or the connection is already closed
        break;
    }

    // Process closing until complete or a timeout is reached
    uint32_t start_time = timer_get_usec();
    uint32_t timeout = 5000000; // 5 second timeout
    int result = 0;

    while (timer_get_usec() - start_time < timeout)
    {
        // Process closing state machine
        result = tcp_process_closing(tcp);

        // If closed or error, break
        if (result != 0)
        {
            break;
        }

        // Small delay to prevent tight loop
        delay_ms(1); // 1ms delay
    }

    // If we didn't fully close after timeout, force close
    if (tcp->state != TCP_CLOSED && timer_get_usec() - start_time >= timeout)
    {
        trace("[%s] Forcing connection close after timeout\n",
              tcp->is_server ? "server" : "client");
        tcp->state = TCP_CLOSED;
    }
}

// Helper function to send a FIN packet
int tcp_send_fin(struct tcp_connection *tcp)
{
    if (!tcp)
        return -1;

    // Create an empty segment with FIN flag
    struct unacked_segment fin_segment = {0};
    fin_segment.seqno = tcp->sender->next_seqno++;
    fin_segment.len = 0; // FIN doesn't carry data
    fin_segment.acked = false;
    fin_segment.send_time = 0;  // Will be set by tcp_send_segment
    fin_segment.is_fin = false; // Will be set to true after adding to window

    // Try to find an empty slot in the sender window for this FIN
    int slot_idx = -1;
    int max_attempts = 10; // Prevent infinite loops
    int attempts = 0;

    while (slot_idx < 0 && attempts < max_attempts)
    {
        // Find an empty slot in the sender window for this FIN
        for (int i = 0; i < SENDER_WINDOW_SIZE; i++)
        {
            if (tcp->sender->segments[i].acked)
            {
                slot_idx = i;
                break;
            }
        }

        if (slot_idx < 0)
        {
            // No room in the window - wait a bit and check for ACKs
            delay_ms(100); // Short delay

            // Check for ACKs to free up window space
            struct rcp_datagram ack = rcp_datagram_init();
            if (tcp_recv_packet(tcp, &ack) == 0 && rcp_has_flag(&ack.header, RCP_FLAG_ACK))
            {
                sender_process_ack(tcp->sender, &ack.header);
            }

            attempts++;
        }
    }

    if (slot_idx < 0)
    {
        trace("[%s] Failed to find space in sender window for FIN after %d attempts\n",
              tcp->is_server ? "server" : "client", max_attempts);
        // In a real implementation we might want to return an error, but for now,
        // we'll just take over the oldest segment slot to avoid deadlock
        slot_idx = 0;
    }

    // Add the FIN to the sender window
    tcp->sender->segments[slot_idx] = fin_segment;

    // Set the FIN flag to indicate this is a special segment
    tcp->sender->segments[slot_idx].is_fin = true;

    // Send the FIN segment using regular segment transmission
    trace("[%s] Sending FIN with seq=%d to %x (via segment)...\n",
          tcp->is_server ? "server" : "client", fin_segment.seqno, tcp->remote_addr);
    return tcp_send_segment(tcp, &tcp->sender->segments[slot_idx]);
}

// Process closing states of a TCP connection
int tcp_process_closing(struct tcp_connection *tcp)
{
    if (!tcp)
        return -1;

    uint8_t buffer[RCP_TOTAL_SIZE];
    int ret;

    // Start the active close if in ESTABLISHED state
    if (tcp->state == TCP_ESTABLISHED)
    {
        trace("[%s] Initiating active close from ESTABLISHED state\n",
              tcp->is_server ? "server" : "client");
        tcp_send_fin(tcp);
        tcp->state = TCP_FIN_WAIT_1;
        tcp->last_time = timer_get_usec();
        return 0;
    }

    // Check for retransmissions using the standard mechanism
    tcp_check_retransmit(tcp, timer_get_usec());

    // Handle each closing state
    switch (tcp->state)
    {
    case TCP_FIN_WAIT_1:
    {
        // Active close: Sent FIN, waiting for ACK and/or FIN

        // Check for response (ACK or FIN)
        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            // Got ACK for our FIN
            if (rcp_has_flag(&rx.header, RCP_FLAG_ACK))
            {
                trace("[%s] Received ACK for FIN in FIN_WAIT_1\n",
                      tcp->is_server ? "server" : "client");

                // Transition depends on whether FIN was also received
                if (rcp_has_flag(&rx.header, RCP_FLAG_FIN))
                {
                    // Got FIN+ACK, send ACK and go to TIME_WAIT
                    trace("[%s] Received FIN+ACK in FIN_WAIT_1, sending ACK\n",
                          tcp->is_server ? "server" : "client");

                    // Send ACK for the FIN
                    struct rcp_datagram ack = rcp_datagram_init();
                    ack.header.src = tcp->sender->src_addr;
                    ack.header.dst = tcp->sender->dst_addr;
                    ack.header.seqno = tcp->sender->next_seqno;
                    ack.header.ackno = rx.header.seqno + 1;
                    rcp_set_flag(&ack.header, RCP_FLAG_ACK);
                    rcp_compute_checksum(&ack.header);

                    if (rcp_datagram_serialize(&ack, buffer, RCP_TOTAL_SIZE) >= 0)
                    {
                        nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                        tcp->state = TCP_TIME_WAIT;
                        tcp->last_time = timer_get_usec(); // Start TIME_WAIT timer
                    }
                }
                else
                {
                    // Got ACK only, transition to FIN_WAIT_2
                    tcp->state = TCP_FIN_WAIT_2;
                }
            }
            // Got FIN but no ACK
            else if (rcp_has_flag(&rx.header, RCP_FLAG_FIN))
            {
                trace("[%s] Received FIN in FIN_WAIT_1, sending ACK\n",
                      tcp->is_server ? "server" : "client");

                // Send ACK for the FIN
                struct rcp_datagram ack = rcp_datagram_init();
                ack.header.src = tcp->sender->src_addr;
                ack.header.dst = tcp->sender->dst_addr;
                ack.header.seqno = tcp->sender->next_seqno;
                ack.header.ackno = rx.header.seqno + 1;
                rcp_set_flag(&ack.header, RCP_FLAG_ACK);
                rcp_compute_checksum(&ack.header);

                if (rcp_datagram_serialize(&ack, buffer, RCP_TOTAL_SIZE) >= 0)
                {
                    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                    tcp->state = TCP_CLOSING; // Simultaneous close
                }
            }
        }
        break;
    }

    case TCP_FIN_WAIT_2:
    {
        // Active close: Received ACK for FIN, waiting for FIN

        // Check for incoming FIN
        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            if (rcp_has_flag(&rx.header, RCP_FLAG_FIN))
            {
                trace("[%s] Received FIN in FIN_WAIT_2, sending ACK\n",
                      tcp->is_server ? "server" : "client");

                // Send ACK for the FIN
                struct rcp_datagram ack = rcp_datagram_init();
                ack.header.src = tcp->sender->src_addr;
                ack.header.dst = tcp->sender->dst_addr;
                ack.header.seqno = tcp->sender->next_seqno;
                ack.header.ackno = rx.header.seqno + 1;
                rcp_set_flag(&ack.header, RCP_FLAG_ACK);
                rcp_compute_checksum(&ack.header);

                if (rcp_datagram_serialize(&ack, buffer, RCP_TOTAL_SIZE) >= 0)
                {
                    nrf_send_noack(tcp->nrf, tcp->remote_addr, buffer, RCP_TOTAL_SIZE);
                    tcp->state = TCP_TIME_WAIT;
                    tcp->last_time = timer_get_usec(); // Start TIME_WAIT timer
                }
            }
        }
        break;
    }

    case TCP_CLOSE_WAIT:
    {
        // Passive close: Already received FIN and sent ACK
        // No special processing needed until application calls tcp_close()
        break;
    }

    case TCP_LAST_ACK:
    {
        // Passive close: Sent FIN, waiting for ACK

        // Check for ACK
        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            if (rcp_has_flag(&rx.header, RCP_FLAG_ACK))
            {
                trace("[%s] Received ACK in LAST_ACK, connection closed\n",
                      tcp->is_server ? "server" : "client");
                tcp->state = TCP_CLOSED;
                return 1; // Fully closed
            }
        }
        break;
    }

    case TCP_CLOSING:
    {
        // Active close: Sent FIN, received FIN but not ACK, waiting for ACK

        // Check for ACK
        ret = nrf_read_exact_timeout(tcp->nrf, buffer, RCP_TOTAL_SIZE, 100);
        if (ret == RCP_TOTAL_SIZE)
        {
            struct rcp_datagram rx = rcp_datagram_init();
            if (rcp_datagram_parse(&rx, buffer, RCP_TOTAL_SIZE) < 0)
                return 0;

            if (rcp_has_flag(&rx.header, RCP_FLAG_ACK))
            {
                trace("[%s] Received ACK in CLOSING, moving to TIME_WAIT\n",
                      tcp->is_server ? "server" : "client");
                tcp->state = TCP_TIME_WAIT;
                tcp->last_time = timer_get_usec(); // Start TIME_WAIT timer
            }
        }
        break;
    }

    case TCP_TIME_WAIT:
    {
        // Either close: waiting for timeout before fully closing
        if (timer_get_usec() - tcp->last_time > TCP_TIME_WAIT_US)
        {
            trace("[%s] TIME_WAIT timeout expired, connection closed\n",
                  tcp->is_server ? "server" : "client");
            tcp->state = TCP_CLOSED;
            return 1; // Fully closed
        }
        break;
    }

    default:
        return -1; // Invalid state
    }

    return 0; // Closing in progress
}