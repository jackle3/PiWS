#include "reassembler.h"

#include <string.h>

struct reassembler *reassembler_init(struct bytestream *out_stream, size_t capacity)
{
    struct reassembler *r = kmalloc(sizeof(*r));
    if (!r)
        return NULL;

    r->output = out_stream;
    r->next_seqno = 0;
    r->capacity = capacity;
    r->bytes_pending = 0;

    // Initialize all slots as empty
    memset(r->segments, 0, sizeof(r->segments));
    for (size_t i = 0; i < RECEIVER_WINDOW_SIZE; i++)
    {
        r->segments[i].data = NULL;
        r->segments[i].received = false;
    }

    return r;
}

static void try_write_in_order(struct reassembler *r)
{
    // Print current state of reassembler buffer
    // trace("[REASM] Buffer state (next_seqno=%d, bytes_pending=%d):\n",
    //       r->next_seqno, r->bytes_pending);
    // for (size_t i = 0; i < RECEIVER_WINDOW_SIZE; i++) {
    //     if (r->segments[i].received) {
    //         trace("  Slot %d: len=%d\n", i, r->segments[i].len);
    //     }
    // }

    // Keep writing segments as long as the next expected one is available
    while (r->segments[0].received)
    {
        // Try to write the segment
        size_t written = bytestream_write(r->output, r->segments[0].data, r->segments[0].len);
        if (written == 0)
        {
            return; // Output stream is full
        }

        // Update state
        r->next_seqno++;
        r->bytes_pending -= r->segments[0].len;

        // Free the slot
        r->segments[0].data = NULL;
        r->segments[0].received = false;

        // Shift all segments left
        for (size_t i = 0; i < RECEIVER_WINDOW_SIZE - 1; i++)
        {
            r->segments[i] = r->segments[i + 1];
        }
        // Clear the last slot
        r->segments[RECEIVER_WINDOW_SIZE - 1].data = NULL;
        r->segments[RECEIVER_WINDOW_SIZE - 1].received = false;
    }
}

size_t reassembler_insert(struct reassembler *r, const uint8_t *data, size_t len, uint16_t seqno,
                          bool is_last)
{
    if (!r || !data || len == 0)
        return 0;

    // If this segment is too old or too far ahead, ignore it
    if (seqno < r->next_seqno || seqno >= r->next_seqno + RECEIVER_WINDOW_SIZE)
    {
        // trace("[REASM] Ignoring segment seq=%d (window: %d-%d)\n",
        //       seqno, r->next_seqno, r->next_seqno + MAX_PENDING_SEGMENTS - 1);
        return 0;
    }

    // Calculate slot index for this sequence number
    size_t slot_index = seqno - r->next_seqno;

    // If this segment is already in our window, ignore it
    if (r->segments[slot_index].received)
    {
        // trace("[REASM] Ignoring duplicate segment seq=%d\n", seqno);
        return 0;
    }

    // If this would put us over capacity, reject it
    if (r->bytes_pending + len > r->capacity)
    {
        // trace("[REASM] Rejecting segment seq=%d: would exceed capacity\n", seqno);
        return 0;
    }

    // Store the segment in its designated slot
    r->segments[slot_index].data = kmalloc(len);
    if (!r->segments[slot_index].data)
        return 0;

    memcpy(r->segments[slot_index].data, data, len);
    r->segments[slot_index].len = len;
    r->segments[slot_index].received = true;
    r->bytes_pending += len;

    // trace("[REASM] Inserted segment seq=%d into slot %d\n", seqno, slot_index);

    // Try to write any segments that are now in order
    try_write_in_order(r);

    return len;
}

uint16_t reassembler_next_seqno(const struct reassembler *r)
{
    return r ? r->next_seqno : 0;
}

size_t reassembler_bytes_pending(const struct reassembler *r)
{
    return r ? r->bytes_pending : 0;
}

bool reassembler_is_complete(const struct reassembler *r)
{
    if (!r)
        return false;

    // Complete if no bytes pending and no segments in buffer
    return r->bytes_pending == 0;
}