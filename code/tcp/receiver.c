#include "receiver.h"
#include "reassembler.h"

#include <string.h>

struct receiver* receiver_init(uint8_t src_addr, uint8_t dst_addr) {
    struct receiver *r = kmalloc(sizeof(struct receiver));
    if (!r) return NULL;

    r->incoming = bytestream_init(RECEIVER_BUFFER_SIZE);
    if (!r->incoming) {
        return NULL;
    }

    r->reasm = reassembler_init(r->incoming, RECEIVER_BUFFER_SIZE);
    if (!r->reasm) {
        return NULL;
    }

    r->window_size = RECEIVER_WINDOW_SIZE;
    r->src_addr = src_addr;
    r->dst_addr = dst_addr;

    return r;
}

int receiver_process_segment(struct receiver *r, const struct rcp_datagram *dgram) {
    if (!r || !dgram) return -1;

    // Verify addresses
    if (dgram->header.dst != r->src_addr || dgram->header.src != r->dst_addr) {
        return -1;
    }

    // Insert segment into reassembler
    size_t bytes_inserted = reassembler_insert(r->reasm, 
                                             dgram->payload,
                                             dgram->header.payload_len,
                                             dgram->header.seqno,
                                             rcp_has_flag(&dgram->header, RCP_FLAG_FIN));

    if (bytes_inserted == 0) {
        return -1;
    }

    // Update window size based on available buffer space
    r->window_size = (RECEIVER_BUFFER_SIZE - bytestream_bytes_available(r->incoming)) 
                    / RCP_MAX_PAYLOAD;
    if (r->window_size == 0) r->window_size = 1;  // Always allow at least one segment

    return 0;
}

void receiver_get_ack(const struct receiver *r, struct rcp_header *ack) {
    if (!r || !ack) return;

    ack->src = r->src_addr;
    ack->dst = r->dst_addr;
    ack->ackno = reassembler_next_seqno(r->reasm) - 1;  // ACK the last in-order segment
    ack->window = r->window_size;
    rcp_set_flag(ack, RCP_FLAG_ACK);
}

size_t receiver_bytes_available(const struct receiver *r) {
    return r ? bytestream_bytes_available(r->incoming) : 0;
}

size_t receiver_read(struct receiver *r, uint8_t *data, size_t len) {
    return r ? bytestream_read(r->incoming, data, len) : 0;
}