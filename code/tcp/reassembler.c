#include "reassembler.h"
#include <string.h>

struct reassembler* reassembler_init(struct bytestream *output, size_t capacity) {
    struct reassembler *r = kmalloc(sizeof(struct reassembler));
    if (!r) return NULL;

    r->output = output;
    r->next_seqno = 0;
    r->capacity = capacity;
    r->bytes_pending = 0;

    // Initialize segments array
    memset(r->segments, 0, sizeof(r->segments));
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        r->segments[i].data = NULL;
        r->segments[i].received = false;
    }

    return r;
}

static void try_write_in_order(struct reassembler *r) {
    bool progress;
    do {
        progress = false;

        // Look for segments that can be written to the output
        for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
            struct pending_segment *seg = &r->segments[i];
            if (seg->received && seg->seqno == r->next_seqno) {
                // Write this segment to the output
                bytestream_write(r->output, seg->data, seg->len);
                
                // Update next expected sequence number
                r->next_seqno++;
                r->bytes_pending -= seg->len;

                // Free segment resources
                seg->data = NULL;
                seg->received = false;
                
                progress = true;
                break;
            }
        }
    } while (progress);
}

size_t reassembler_insert(struct reassembler *r, const uint8_t *data, 
                         size_t len, uint16_t seqno, bool is_last) {
    if (!r || !data || len == 0) return 0;

    // If this is an old segment we've already processed, ignore it
    if (seqno < r->next_seqno) return 0;

    // Check if we have capacity for this segment
    if (r->bytes_pending + len > r->capacity) return 0;

    // Find a slot for this segment
    struct pending_segment *target = NULL;
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        struct pending_segment *seg = &r->segments[i];
        if (!seg->received) {
            target = seg;
            break;
        }
    }

    if (!target) return 0;  // No space for new segments

    // Allocate space for the segment data
    target->data = kmalloc(len);
    if (!target->data) return 0;

    // Store the segment
    memcpy(target->data, data, len);
    target->len = len;
    target->seqno = seqno;
    target->received = true;
    r->bytes_pending += len;

    // Try to write any segments that are now in order
    try_write_in_order(r);

    return len;
}

uint16_t reassembler_next_seqno(const struct reassembler *r) {
    return r ? r->next_seqno : 0;
}

size_t reassembler_bytes_pending(const struct reassembler *r) {
    return r ? r->bytes_pending : 0;
}

bool reassembler_is_complete(const struct reassembler *r) {
    if (!r) return false;

    // Check if we have any pending segments
    if (r->bytes_pending > 0) return false;

    // Check if all segments have been processed
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        if (r->segments[i].received) return false;
    }

    return true;
}