#include "receiver.h"

#include <string.h>

#include "reassembler.h"

struct receiver *receiver_init(uint8_t src_addr, uint8_t dst_addr) {
    struct receiver *r = kmalloc(sizeof(struct receiver));
    if (!r)
        return NULL;

    // Create bytestream for storing reassembled data
    r->incoming = bytestream_init(RECEIVER_BUFFER_SIZE);
    if (!r->incoming) {
        return NULL;
    }

    // Create reassembler to handle out-of-order segments
    r->reasm = reassembler_init(r->incoming, RECEIVER_BUFFER_SIZE);
    if (!r->reasm) {
        return NULL;
    }

    r->window_size = RECEIVER_WINDOW_SIZE;  // Initial receive window size
    r->src_addr = src_addr;                 // Our address
    r->dst_addr = dst_addr;                 // Remote sender's address

    return r;
}

int receiver_process_segment(struct receiver *r, const struct rcp_datagram *dgram) {
    if (!r || !dgram)
        return -1;

    // Verify source and destination addresses match expected
    if (dgram->header.dst != r->src_addr || dgram->header.src != r->dst_addr) {
        return -1;
    }

    // If this is a retransmission of an already processed segment
    // (sequence number is less than what we expect next),
    // we should still generate an ACK to handle the case of lost ACKs
    trace("[REASM] Received segment seq=%d, next_seqno=%d\n", dgram->header.seqno,
          reassembler_next_seqno(r->reasm));
    if (dgram->header.seqno < reassembler_next_seqno(r->reasm)) {
        trace("Received retransmission of already processed segment seq=%d\n", dgram->header.seqno);
        return 1;  // Return 1 to indicate ACK should be sent but no new data
    }

    // Insert segment into reassembler to be buffered and reassembled
    size_t bytes_inserted =
        reassembler_insert(r->reasm, dgram->payload, dgram->header.payload_len, dgram->header.seqno,
                           rcp_has_flag(&dgram->header, RCP_FLAG_FIN));

    // Return error if reassembler couldn't process segment
    if (bytes_inserted == 0) {
        trace("Failed to insert segment into reassembler, buffer is full\n");
        return -1;
    }

    // Update receive window based on available buffer space
    // Window size is in segments (bytes / max payload size)
    r->window_size =
        (RECEIVER_BUFFER_SIZE - bytestream_bytes_available(r->incoming)) / RCP_MAX_PAYLOAD;
    if (r->window_size == 0)
        r->window_size = 1;  // Always allow at least one segment

    return 0;  // Return 0 to indicate new data was processed
}

void receiver_get_ack(const struct receiver *r, struct rcp_header *ack) {
    if (!r || !ack)
        return;

    ack->src = r->src_addr;                             // Our address
    ack->dst = r->dst_addr;                             // Remote sender's address
    ack->ackno = reassembler_next_seqno(r->reasm) - 1;  // ACK the last in-order segment
    ack->window = r->window_size;                       // Current receive window
    rcp_set_flag(ack, RCP_FLAG_ACK);                    // Mark as ACK packet
}

size_t receiver_bytes_available(const struct receiver *r) {
    // Return number of bytes available to read from reassembled stream
    return r ? bytestream_bytes_available(r->incoming) : 0;
}

size_t receiver_read(struct receiver *r, uint8_t *data, size_t len) {
    // Read bytes from reassembled stream
    return r ? bytestream_read(r->incoming, data, len) : 0;
}