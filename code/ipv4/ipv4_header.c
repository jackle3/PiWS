#include "ipv4_header.h"

/* Calculate the payload length from the total length field */
uint16_t ipv4_payload_length(const struct ipv4_header *hdr) {
    /* Total length - header length (IHL * 4 bytes) */
    return hdr->len - (hdr->hlen * 4);
}

/*
 * Calculate the pseudo header checksum used in TCP/UDP checksum calculations.
 * 
 * The pseudo-header is a security measure to ensure TCP/UDP segments weren't
 * misdelivered due to IP header corruption. It's NOT stored in the packet,
 * but is used as part of TCP/UDP checksum calculation.
 *
 * Pseudo-header structure (12 bytes total):
 * - Source IP (4 bytes)      : Ensures segment came from correct source
 * - Destination IP (4 bytes) : Ensures segment goes to correct destination
 * - Protocol (2 bytes)       : Ensures correct protocol handling
 * - TCP/UDP length (2 bytes) : Ensures complete segment coverage
 *
 * This is different from the IP header checksum, which only protects
 * the IP header itself and is stored in the header.
 */
uint32_t ipv4_pseudo_checksum(const struct ipv4_header *hdr) {
    uint32_t sum = 0;
    
    /* Add source and destination addresses (split into 16-bit chunks) */
    sum += (hdr->src >> 16) & 0xFFFF;  /* High 16 bits of source IP */
    sum += hdr->src & 0xFFFF;          /* Low 16 bits of source IP */
    sum += (hdr->dst >> 16) & 0xFFFF;  /* High 16 bits of dest IP */
    sum += hdr->dst & 0xFFFF;          /* Low 16 bits of dest IP */
    
    /* Add protocol (padded to 16 bits)
     * Protocol is only 8 bits but needs to be 16 for checksum */
    sum += (uint16_t)hdr->proto;
    
    /* Add TCP/UDP segment length
     * This ensures the checksum covers the entire transport segment */
    sum += ipv4_payload_length(hdr);
    
    return sum;
}

/* 
 * Helper function to calculate 16-bit one's complement sum
 * Used by both regular IP checksum and TCP/UDP checksums
 *
 * The Internet checksum is designed to:
 * 1. Detect common error patterns (bit flips, byte swaps)
 * 2. Be easy to compute incrementally
 * 3. Be position-independent
 */
uint16_t ones_complement_sum(const uint16_t *data, size_t len_bytes) {
    uint32_t sum = 0;
    size_t i;
    
    /* Sum up 16-bit words
     * We use 32-bit sum to handle carries properly */
    for (i = 0; i < len_bytes / 2; i++) {
        sum += data[i];
        /* Fold carry bits back into the sum */
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    
    /* If there's an odd byte, add it padded with zero
     * This ensures all data is included in checksum */
    if (len_bytes & 1) {
        sum += ((uint8_t*)data)[len_bytes - 1];
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    
    /* Fold any remaining carries into 16 bits
     * This could take multiple iterations */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)sum;
}

/*
 * Compute the IPv4 header checksum
 *
 * This checksum ONLY protects the IP header and is verified by every
 * router along the path. It's different from the pseudo-header checksum
 * used in TCP/UDP calculations.
 *
 * Properties:
 * 1. Stored in the IP header itself
 * 2. Covers only the IP header (not the payload)
 * 3. Must be recomputed by routers when they modify the header (e.g., TTL)
 */
void ipv4_compute_checksum(struct ipv4_header *hdr) {
    /* Zero out the checksum field for calculation
     * The checksum field must be zero during computation */
    hdr->cksum = 0;
    
    /* Calculate checksum over header only (hlen * 4 bytes)
     * Note: This is different from pseudo-header checksum which
     * is used for TCP/UDP checksums */
    uint16_t sum = ones_complement_sum((uint16_t*)hdr, hdr->hlen * 4);
    
    /* Store one's complement of sum
     * A correct checksum will sum to 0xFFFF when verified */
    hdr->cksum = ~sum;
}

/* Parse raw network data into an IPv4 header structure */
void ipv4_parse(struct ipv4_header *hdr, void *parser) {
    uint8_t *data = (uint8_t*)parser;
    
    /* First byte contains version and header length */
    hdr->ver = (data[0] >> 4) & 0x0F;
    hdr->hlen = data[0] & 0x0F;
    
    /* Parse remaining fields */
    hdr->tos = data[1];
    hdr->len = (data[2] << 8) | data[3];
    hdr->id = (data[4] << 8) | data[5];
    
    /* Parse flags and fragment offset from 3 bytes */
    uint16_t flags_frag = (data[6] << 8) | data[7];
    hdr->df = (flags_frag >> 14) & 0x1;
    hdr->mf = (flags_frag >> 13) & 0x1;
    hdr->offset = flags_frag & 0x1FFF;
    
    hdr->ttl = data[8];
    hdr->proto = data[9];
    /* Note: Checksum field is part of header integrity verification */
    hdr->cksum = (data[10] << 8) | data[11];
    
    /* Source and destination addresses
     * These are also used in pseudo-header checksum for TCP/UDP */
    hdr->src = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];
    hdr->dst = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
}

/* Serialize an IPv4 header structure into network data */
void ipv4_serialize(const struct ipv4_header *hdr, void *serializer) {
    uint8_t *data = (uint8_t*)serializer;
    
    /* Version and header length */
    data[0] = (hdr->ver << 4) | (hdr->hlen & 0x0F);
    
    /* Rest of the header fields */
    data[1] = hdr->tos;
    data[2] = (hdr->len >> 8) & 0xFF;
    data[3] = hdr->len & 0xFF;
    data[4] = (hdr->id >> 8) & 0xFF;
    data[5] = hdr->id & 0xFF;
    
    /* Flags and fragment offset */
    uint16_t flags_frag = (hdr->df << 14) | (hdr->mf << 13) | (hdr->offset & 0x1FFF);
    data[6] = (flags_frag >> 8) & 0xFF;
    data[7] = flags_frag & 0xFF;
    
    data[8] = hdr->ttl;
    data[9] = hdr->proto;
    /* Include the checksum in serialized output */
    data[10] = (hdr->cksum >> 8) & 0xFF;
    data[11] = hdr->cksum & 0xFF;
    
    /* Source address */
    data[12] = (hdr->src >> 24) & 0xFF;
    data[13] = (hdr->src >> 16) & 0xFF;
    data[14] = (hdr->src >> 8) & 0xFF;
    data[15] = hdr->src & 0xFF;
    
    /* Destination address */
    data[16] = (hdr->dst >> 24) & 0xFF;
    data[17] = (hdr->dst >> 16) & 0xFF;
    data[18] = (hdr->dst >> 8) & 0xFF;
    data[19] = hdr->dst & 0xFF;
}