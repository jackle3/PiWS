#include "reassembler.h"

#include <string.h>

struct reassembler *reassembler_init(struct bytestream *out_stream, size_t capacity) {
    // Allocate space for reassembler struct
    struct reassembler *r = kmalloc(sizeof(struct reassembler));
    if (!r)
        return NULL;

    // Initialize fields
    r->output = out_stream;  // Output stream to write reassembled data to
    r->next_seqno = 0;       // Next expected sequence number
    r->capacity = capacity;  // Max bytes that can be buffered
    r->bytes_pending = 0;    // Current bytes buffered but not written

    // Initialize all segment slots as empty
    memset(r->segments, 0, sizeof(r->segments));
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        r->segments[i].data = NULL;       // No data buffer allocated yet
        r->segments[i].received = false;  // Slot is available
    }

    return r;
}

static void try_write_in_order(struct reassembler *r) {
    // Whether we wrote any segments to the output bytestream.
    // If we didn't, we can stop flushing buffer.
    bool wrote_something;

    // Keep trying to write segments as long as there are segments in order (i.e. seqno matches
    // next_seqno)
    do {
        wrote_something = false;

        // Look through all slots for segments we can write
        for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
            struct pending_segment *seg = &r->segments[i];

            // If this slot is empty, ignore it
            if (!seg->received)
                continue;

            // If this slot is not in order (i.e. seqno doesn't match what we want to flush next),
            // ignore
            if (seg->seqno != r->next_seqno)
                continue;

            // Found next segment in sequence - try writing to output stream
            size_t written = bytestream_write(r->output, seg->data, seg->len);
            if (written == 0) {
                return;  // Output stream is full
            }

            // Successfully wrote segment, update state
            r->next_seqno++;               // Advance expected sequence number
            r->bytes_pending -= seg->len;  // Reduce pending byte count

            // Free segment slot
            seg->data = NULL;       // Free data buffer
            seg->received = false;  // Mark slot as available

            wrote_something = true;  // Note that we wrote a segment

            // Found and wrote a segment, start over looking for next in sequence
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
    return r ? r->next_seqno : 0;  // Return next expected seqno, or 0 if invalid
}

size_t reassembler_bytes_pending(const struct reassembler *r) {
    return r ? r->bytes_pending : 0;  // Return bytes pending, or 0 if invalid
}

bool reassembler_is_complete(const struct reassembler *r) {
    if (!r)
        return false;

    // Not complete if we have bytes pending
    if (r->bytes_pending > 0)
        return false;

    // Not complete if any segments still buffered
    for (size_t i = 0; i < MAX_PENDING_SEGMENTS; i++) {
        if (r->segments[i].received)
            return false;
    }

    // Complete if no pending bytes and no buffered segments
    return true;
}