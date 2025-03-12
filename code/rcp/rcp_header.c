#include "rcp_header.h"
#include <string.h>

struct rcp_header rcp_header_init(void) {
    struct rcp_header hdr = {
        .payload_len = 0,
        .cksum = 0,
        .dst = 0,
        .src = 0,
        .seqno = 0,
        .flags = 0,
        .ackno = 0,
        .window = 0
    };
    return hdr;
}

/* Simple 8-bit checksum calculation */
static uint8_t calculate_checksum(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum + 1;  // Two's complement
}

void rcp_compute_checksum(struct rcp_header *hdr) {
    // Save and zero checksum field
    uint8_t saved_cksum = hdr->cksum;
    hdr->cksum = 0;
    
    // Calculate checksum over header
    uint8_t checksum = calculate_checksum((uint8_t*)hdr, RCP_HEADER_LENGTH);
    hdr->cksum = checksum;
}

void rcp_parse(struct rcp_header *hdr, const void *data) {
    const uint8_t *bytes = data;
    
    hdr->payload_len = bytes[0];
    hdr->cksum = bytes[1];
    hdr->dst = bytes[2];
    hdr->src = bytes[3];
    hdr->seqno = (bytes[4] << 8) | bytes[5];
    hdr->flags = bytes[6];
    hdr->ackno = (bytes[7] << 8) | bytes[8];
    hdr->window = bytes[9];
}

void rcp_serialize(const struct rcp_header *hdr, void *data) {
    uint8_t *bytes = data;
    
    bytes[0] = hdr->payload_len;
    bytes[1] = hdr->cksum;
    bytes[2] = hdr->dst;
    bytes[3] = hdr->src;
    bytes[4] = (hdr->seqno >> 8) & 0xFF;
    bytes[5] = hdr->seqno & 0xFF;
    bytes[6] = hdr->flags;
    bytes[7] = (hdr->ackno >> 8) & 0xFF;
    bytes[8] = hdr->ackno & 0xFF;
    bytes[9] = hdr->window;
} 