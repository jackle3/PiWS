#pragma once

#include "rpi.h"
#include <stddef.h>
#include <stdint.h>

#define IPV4_HEADER_LENGTH 20        /* IPv4 header length, not including options */
#define IPV4_DEFAULT_TTL 128         /* A reasonable default TTL value */
#define IPV4_PROTO_TCP 6             /* Protocol number for TCP */

/*
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |Version|  IHL  |Type of Service|          Total Length         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |         Identification        |Flags|      Fragment Offset    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |  Time to Live |    Protocol   |         Header Checksum       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                       Source Address                          |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                    Destination Address                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                    Options                    |    Padding    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct ipv4_header {
    uint8_t ver;           /* IP version */ 
    uint8_t hlen;          /* header length (multiples of 32 bits) */
    uint8_t tos;           /* type of service */
    uint16_t len;          /* total length of packet */
    uint16_t id;           /* identification number */
    uint8_t df;            /* don't fragment flag */
    uint8_t mf;            /* more fragments flag */
    uint16_t offset;       /* fragment offset field */
    uint8_t ttl;          /* time to live field */
    uint8_t proto;        /* protocol field */
    uint16_t cksum;       /* checksum field */
    uint32_t src;         /* src address */
    uint32_t dst;         /* dst address */
};

/* Function prototypes */
uint16_t ipv4_payload_length(const struct ipv4_header *hdr);
uint32_t ipv4_pseudo_checksum(const struct ipv4_header *hdr);
void ipv4_compute_checksum(struct ipv4_header *hdr);
void ipv4_parse(struct ipv4_header *hdr, const void *data_bytes);
void ipv4_serialize(const struct ipv4_header *hdr, void *serializer);

struct ipv4_header ipv4_init(void);

/* Calculate 16-bit one's complement sum over data */
uint16_t ones_complement_sum(const uint16_t *data, size_t len_bytes);