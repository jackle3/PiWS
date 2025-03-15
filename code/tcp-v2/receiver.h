#pragma once

#include "bytestream.h"
#include "nrf.h"
#include "segments.h"

#define REASM_BUFFER_SIZE 65536  // reassembler can hold 64KB of out-of-order segments

typedef struct receiver {
    nrf_t *nrf;           // Receiver's NRF interface (to receive segments)
    bytestream_t writer;  // Receiver writes to it, app reads from it

    char reasm_buffer[REASM_BUFFER_SIZE];   // Buffer for reassembled data
    char reasm_bitmask[REASM_BUFFER_SIZE];  // Bitmask to track received segments

    uint16_t total_bytes_received;  // Total bytes received
    bool fin_received;              // Whether a FIN has been received

    void (*transmit)(receiver_segment_t *segment);  // Callback to send ACKs to the remote peer
} receiver_t;

receiver_t receiver_init(nrf_t *nrf, void (*transmit)(receiver_segment_t *segment)) {
    receiver_t receiver;
    receiver.nrf = nrf;
    receiver.writer = bs_init();
    memset(receiver.reasm_buffer, 0, REASM_BUFFER_SIZE);
    memset(receiver.reasm_bitmask, 0, REASM_BUFFER_SIZE);

    receiver.total_bytes_received = 0;
    receiver.fin_received = false;
    receiver.transmit = transmit;
    return receiver;
}