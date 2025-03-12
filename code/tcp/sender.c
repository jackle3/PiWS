#include "sender.h"
#include <string.h>

struct sender* sender_init(uint8_t src_addr, uint8_t dst_addr, size_t stream_capacity) {
    struct sender *s = kmalloc(sizeof(struct sender));
    if (!s) return NULL;

    s->outgoing = bytestream_init(stream_capacity);
    if (!s->outgoing) {
        return NULL;
    }

    s->next_seqno = 0;
    s->window_size = SENDER_WINDOW_SIZE;
    s->segments_in_flight = 0;
    s->src_addr = src_addr;
    s->dst_addr = dst_addr;

    // Initialize segments array
    memset(s->segments, 0, sizeof(s->segments));
    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++) {
        s->segments[i].acked = true;  // Mark as acked so they're available
    }

    return s;
}

/**
 * Fills the sender's window with new segments from the outgoing bytestream.
 * 
 * This function reads all available data from the outgoing bytestream and breaks it
 * into segments of size RCP_MAX_PAYLOAD. For each segment, it creates an unacked_segment
 * structure to track it until acknowledgment is received.
 *
 * The function continues creating segments as long as:
 * 1. There is space available in the sliding window (segments_in_flight < window_size)
 * 2. There is data available in the outgoing bytestream
 * 3. There are free segment slots available
 *
 * @param s Pointer to the sender struct
 * @return Number of new segments created, or -1 on error
 */
int sender_fill_window(struct sender *s) {
    // Input validation
    if (!s) return -1;

    int segments_created = 0;
    
    // Keep creating segments while we have:
    // - Space in the window (segments_in_flight < window_size)
    // - Data to send in the bytestream
    while (s->segments_in_flight < s->window_size && 
           bytestream_bytes_available(s->outgoing) > 0) {
        
        // Search for an unused segment slot in our segments array
        // (unused slots are marked as acked=true)
        struct unacked_segment *seg = NULL;
        size_t slot;
        for (slot = 0; slot < SENDER_WINDOW_SIZE; slot++) {
            if (s->segments[slot].acked) {
                seg = &s->segments[slot];
                break;
            }
        }
        // If no free slots found, stop creating segments
        if (!seg) break;  

        // Read up to RCP_MAX_PAYLOAD bytes from the bytestream into this segment
        // This effectively breaks the stream into fixed-size chunks
        size_t bytes_read = bytestream_read(s->outgoing, 
                                          seg->data, 
                                          RCP_MAX_PAYLOAD);
        if (bytes_read == 0) break;

        // Setup the new unacked segment with:
        // - The actual data length we read
        // - The next sequence number
        // - Mark it as not acknowledged
        // - Clear send time (will be set when actually transmitted)
        seg->len = bytes_read;
        seg->seqno = s->next_seqno++;
        seg->acked = false;
        seg->send_time = 0;  

        // Update window tracking
        s->segments_in_flight++;
        segments_created++;
    }

    return segments_created;
}

int sender_process_ack(struct sender *s, const struct rcp_header *ack) {
    if (!s || !ack) return -1;

    int segments_acked = 0;

    // Process cumulative ACK
    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++) {
        struct unacked_segment *seg = &s->segments[i];
        if (!seg->acked && seg->seqno <= ack->ackno) {
            seg->acked = true;
            s->segments_in_flight--;
            segments_acked++;
        }
    }

    // Update window size
    s->window_size = ack->window;

    return segments_acked;
}

int sender_check_retransmit(struct sender *s, uint32_t current_time_ms) {
    if (!s) return -1;

    int segments_to_retransmit = 0;

    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++) {
        struct unacked_segment *seg = &s->segments[i];
        if (!seg->acked && seg->send_time > 0 &&
            (current_time_ms - seg->send_time) >= RETRANSMIT_TIMEOUT_MS) {
            // Mark for retransmission by clearing send time
            seg->send_time = 0;
            segments_to_retransmit++;
        }
    }

    return segments_to_retransmit;
}

const struct unacked_segment* sender_next_segment(const struct sender *s) {
    if (!s) return NULL;

    // First look for segments that need retransmission
    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++) {
        const struct unacked_segment *seg = &s->segments[i];
        if (!seg->acked && seg->send_time == 0) {
            return seg;
        }
    }

    return NULL;
}

void sender_segment_sent(struct sender *s, const struct unacked_segment *seg, 
                        uint32_t current_time_ms) {
    if (!s || !seg) return;

    // Find the segment in our array and update its send time
    for (size_t i = 0; i < SENDER_WINDOW_SIZE; i++) {
        if (&s->segments[i] == seg) {
            s->segments[i].send_time = current_time_ms;
            break;
        }
    }
}