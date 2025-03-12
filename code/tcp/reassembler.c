#include "reassembler.h"

#include <string.h>

struct reassembler *reassembler_init(struct bytestream *out_stream, size_t capacity) {
    struct reassembler *r = kmalloc(sizeof(struct reassembler));
    if (!r)
        return NULL;

    r->output = out_stream;
    r->next_seqno = 0;
    r->capacity = capacity;
    r->bytes_pending = 0;

    // Initialize all slots as empty
    memset(r->segments, 0, sizeof(r->segments));
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        r->segments[i].data = NULL;
        r->segments[i].received = false;
    }

    return r;
}

static void try_write_in_order(struct reassembler *r) {
    // Whether we wrote any segments to the output bytestream.
    // If we didn't, we can stop flushing buffer.
    bool wrote_something;

    // Keep trying to write segments as long as there are segments in order (i.e. seqno matches next_seqno)
    do {
        wrote_something = false;

        // Look through all slots for segments we can write
        for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
            struct pending_segment *seg = &r->segments[i];

            // If this slot is empty, ignore it
            if (!seg->received) continue;

            // If this slot is not in order (i.e. seqno doesn't match what we want to flush next), ignore
            if (seg->seqno != r->next_seqno) continue;

            // Found a segment that's next in sequence! Write it to the output stream
            size_t written = bytestream_write(r->output, seg->data, seg->len);
            if (written == 0) {
                return; // No more space in output stream
            }

            // Update next expected sequence number
            r->next_seqno++;
            r->bytes_pending -= seg->len;

            // Free segment resources
            seg->data = NULL;
            seg->received = false;

            wrote_something = true;

            // We found a segment that we can write, reset and look for next segment in order
            break;
        }
    } while (wrote_something);
}

size_t reassembler_insert(struct reassembler *r, const uint8_t *data, size_t len, uint16_t seqno,
                          bool is_last) {
    if (!r || !data || len == 0)
        return 0;

    // If this segment is too old (we've already processed past it), ignore it
    if (seqno < r->next_seqno)
        return 0;

    // If this would put us over capacity, reject it
    if (r->bytes_pending + len > r->capacity)
        return 0;

    // Find an empty slot (i.e. space in reassembler buffer) that we can use to store this segment
    struct pending_segment *empty_slot = NULL;
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        struct pending_segment *seg = &r->segments[i];
        if (!seg->received) {
            empty_slot = seg;
            break;
        }
    }
    if (!empty_slot)
        return 0;  // No slots in reassembler buffer to store new segment

    // Copy the segment data into the empty slot in the reassembler buffer
    empty_slot->data = kmalloc(len);
    if (!empty_slot->data)
        return 0;
    memcpy(empty_slot->data, data, len);
    empty_slot->len = len;
    empty_slot->seqno = seqno;
    empty_slot->received = true;
    r->bytes_pending += len;  // Update the total bytes pending (add this segment's length)

    // Flush the buffer by trying to write any segments that are now in order
    try_write_in_order(r);

    // Return the number of bytes successfully inserted into reassembler
    return len;
}

uint16_t reassembler_next_seqno(const struct reassembler *r) {
    return r ? r->next_seqno : 0;
}

size_t reassembler_bytes_pending(const struct reassembler *r) {
    return r ? r->bytes_pending : 0;
}

bool reassembler_is_complete(const struct reassembler *r) {
    if (!r)
        return false;

    // Check if we have any pending segments
    if (r->bytes_pending > 0)
        return false;

    // Check if all segments have been processed
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        if (r->segments[i].received)
            return false;
    }

    return true;
}