#pragma once

#include "rpi.h"
#include <stddef.h>
#include <stdint.h>

#define RCP_HEADER_LENGTH 10 /* RCP header length in bytes */
#define RCP_MAX_PAYLOAD 22   /* Maximum payload size to fit in 32-byte NRF packet */
#define RCP_TOTAL_SIZE 32    /* Total size of RCP packet (header + max payload) */

/* Flag bits for the flags field */
#define RCP_FLAG_FIN (1 << 0) /* FIN flag */
#define RCP_FLAG_SYN (1 << 1) /* SYN flag */
#define RCP_FLAG_ACK (1 << 2) /* ACK flag */

/*
 * RCP Header Format (10 bytes total):
 * Byte 0:     Payload Length (1 byte)
 * Byte 1:     Checksum (1 byte)
 * Byte 2:     Destination Address (1 byte)
 * Byte 3:     Source Address (1 byte)
 * Bytes 4-5:  Sequence Number (2 bytes)
 * Byte 6:     Flags (FIN, SYN, ACK) (1 byte)
 * Bytes 7-8:  Acknowledgment Number (2 bytes)
 * Byte 9:     Window Size (1 byte)
 */
struct rcp_header
{
    uint8_t payload_len; /* Length of payload */
    uint8_t cksum;       /* Header checksum */
    uint8_t dst;         /* Destination address */
    uint8_t src;         /* Source address */
    uint16_t seqno;      /* Sequence number */
    uint8_t flags;       /* Control flags (FIN, SYN, ACK) */
    uint16_t ackno;      /* Acknowledgment number */
    uint8_t window;      /* Window size */
};

/* Initialize RCP header with default values */
struct rcp_header rcp_header_init(void);

/* Compute the header checksum */
void rcp_compute_checksum(struct rcp_header *hdr);

/* Parse raw network data into an RCP header structure */
void rcp_parse(struct rcp_header *hdr, const void *data);

/* Serialize an RCP header structure into network data */
void rcp_serialize(const struct rcp_header *hdr, void *data);

/* Helper functions for flag manipulation */
static inline void rcp_set_flag(struct rcp_header *hdr, uint8_t flag)
{
    hdr->flags |= flag;
}

static inline void rcp_clear_flag(struct rcp_header *hdr, uint8_t flag)
{
    hdr->flags &= ~flag;
}

static inline int rcp_has_flag(const struct rcp_header *hdr, uint8_t flag)
{
    return (hdr->flags & flag) != 0;
}

char *rcp_to_string(uint8_t rcp_addr);