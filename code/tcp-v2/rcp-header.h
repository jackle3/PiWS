#pragma once

#include "rpi.h"

#define RCP_HEADER_LENGTH 11 /* RCP header length in bytes */
#define RCP_MAX_PAYLOAD 21   /* Maximum payload size to fit in 32-byte packet */
#define RCP_TOTAL_SIZE 32    /* Total size of RCP packet (header + max payload) */

/* Flag bits for the flags field */
#define RCP_FLAG_FIN (1 << 0) /* FIN flag */
#define RCP_FLAG_SYN (1 << 1) /* SYN flag */
#define RCP_FLAG_ACK (1 << 2) /* ACK flag */

/*
 * RCP Header Format (11 bytes total):
 * Byte 0:     Payload Length (1 byte)
 * Byte 1:     Checksum (1 byte)
 * Byte 2:     Destination Address (1 byte)
 * Byte 3:     Source Address (1 byte)
 * Bytes 4-5:  Sequence Number (2 bytes)
 * Byte 6:     Flags (FIN, SYN, ACK) (1 byte)
 * Bytes 7-8:  Acknowledgment Number (2 bytes)
 * Bytes 9-10: Window Size (2 bytes)
 */
typedef struct rcp_header {
    uint8_t payload_len; /* Length of payload */
    uint8_t cksum;       /* Checksum covering header and payload */
    uint8_t dst;         /* Destination address */
    uint8_t src;         /* Source address */
    uint16_t seqno;      /* Sequence number */
    uint8_t flags;       /* Control flags (FIN, SYN, ACK) */
    uint16_t ackno;      /* Acknowledgment number */
    uint16_t window;     /* Window size */
} rcp_header_t;

/* Forward declarations for inline functions */
static inline rcp_header_t rcp_header_init(void);
static inline uint8_t rcp_calculate_checksum(const rcp_header_t *hdr, const uint8_t *payload);
static inline void rcp_compute_checksum(rcp_header_t *hdr, const uint8_t *payload);
static inline int rcp_verify_checksum(const rcp_header_t *hdr, const uint8_t *payload);
static inline void rcp_header_parse(rcp_header_t *hdr, const void *data);
static inline void rcp_header_serialize(const rcp_header_t *hdr, void *data);

/* Helper functions for flag manipulation */
static inline void rcp_set_flag(rcp_header_t *hdr, uint8_t flag) { hdr->flags |= flag; }

static inline void rcp_clear_flag(rcp_header_t *hdr, uint8_t flag) { hdr->flags &= ~flag; }

static inline int rcp_has_flag(const rcp_header_t *hdr, uint8_t flag) {
    return (hdr->flags & flag) != 0;
}

/* Initialize RCP header with default values */
static inline rcp_header_t rcp_header_init(void) {
    rcp_header_t hdr = {.payload_len = 0,
                        .cksum = 0,
                        .dst = 0,
                        .src = 0,
                        .seqno = 0,
                        .flags = 0,
                        .ackno = 0,
                        .window = 0};
    return hdr;
}

/*
 * 16-bit one's complement sum checksum calculation
 * Similar to TCP/IP checksum but simplified for RCP
 */
static inline uint8_t rcp_calculate_checksum(const rcp_header_t *hdr, const uint8_t *payload) {
    if (!hdr) {
        return 0;
    }

    // Create a working copy of the header with checksum field zeroed
    rcp_header_t temp_hdr = *hdr;
    temp_hdr.cksum = 0;

    // Use 16-bit one's complement sum
    uint16_t sum = 0;
    const uint8_t *data = (const uint8_t *)&temp_hdr;

    // Sum header bytes as 16-bit words
    for (size_t i = 0; i < RCP_HEADER_LENGTH; i += 2) {
        if (i + 1 < RCP_HEADER_LENGTH) {
            sum += (data[i] << 8) | data[i + 1];
        } else {
            // Handle odd byte count
            sum += (data[i] << 8);
        }

        // Add carry
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    // Add payload bytes if present
    if (payload && hdr->payload_len > 0) {
        for (size_t i = 0; i < hdr->payload_len; i += 2) {
            if (i + 1 < hdr->payload_len) {
                sum += (payload[i] << 8) | payload[i + 1];
            } else {
                // Handle odd byte count
                sum += (payload[i] << 8);
            }

            // Add carry
            while (sum >> 16) {
                sum = (sum & 0xFFFF) + (sum >> 16);
            }
        }
    }

    // Take one's complement
    sum = ~sum;

    // Return 8-bit checksum (fold the 16-bit value)
    return (uint8_t)((sum & 0xFF) + ((sum >> 8) & 0xFF));
}

/*
 * Compute and set the header checksum in the header struct
 * This will calculate checksum over header and payload (if provided)
 */
static inline void rcp_compute_checksum(rcp_header_t *hdr, const uint8_t *payload) {
    if (!hdr) {
        return;
    }

    // Calculate checksum with the checksum field zeroed
    hdr->cksum = 0;
    hdr->cksum = rcp_calculate_checksum(hdr, payload);
}

/*
 * Verify the checksum of header and payload
 * Returns 1 if valid, 0 if invalid
 */
static inline int rcp_verify_checksum(const rcp_header_t *hdr, const uint8_t *payload) {
    if (!hdr) {
        return 0;
    }

    // Save original checksum
    uint8_t original_cksum = hdr->cksum;

    // Create a copy of the header
    rcp_header_t temp_hdr = *hdr;
    temp_hdr.cksum = 0;

    // Calculate checksum with zeroed checksum field
    uint8_t calculated_cksum = rcp_calculate_checksum(&temp_hdr, payload);

    // If checksum is valid, they should match
    return (calculated_cksum == original_cksum) ? 1 : 0;
}

/* Parse raw network data into an RCP header structure */
static inline void rcp_header_parse(rcp_header_t *hdr, const void *data) {
    if (!hdr || !data) {
        return;
    }

    const uint8_t *bytes = (const uint8_t *)data;

    hdr->payload_len = bytes[0];
    hdr->cksum = bytes[1];
    hdr->dst = bytes[2];
    hdr->src = bytes[3];
    hdr->seqno = (bytes[4] << 8) | bytes[5];
    hdr->flags = bytes[6];
    hdr->ackno = (bytes[7] << 8) | bytes[8];
    hdr->window = (bytes[9] << 8) | bytes[10];
}

/* Serialize an RCP header structure into network data */
static inline void rcp_header_serialize(const rcp_header_t *hdr, void *data) {
    if (!hdr || !data) {
        return;
    }

    uint8_t *bytes = (uint8_t *)data;

    bytes[0] = hdr->payload_len;
    bytes[1] = hdr->cksum;
    bytes[2] = hdr->dst;
    bytes[3] = hdr->src;
    bytes[4] = (hdr->seqno >> 8) & 0xFF;
    bytes[5] = hdr->seqno & 0xFF;
    bytes[6] = hdr->flags;
    bytes[7] = (hdr->ackno >> 8) & 0xFF;
    bytes[8] = hdr->ackno & 0xFF;
    bytes[9] = (hdr->window >> 8) & 0xFF;
    bytes[10] = hdr->window & 0xFF;
}