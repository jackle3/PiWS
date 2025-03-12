#pragma once

#include "ipv4_header.h"
#include <stddef.h>
#include <stdint.h>

/* IPv4 Datagram structure
 * Contains both the header and payload of an IPv4 packet
 */
struct ipv4_datagram {
    struct ipv4_header header;  /* IPv4 header */
    uint8_t* payload;          /* Pointer to payload data */
    size_t payload_length;     /* Length of payload in bytes */
};

/* Initialize a new IPv4 datagram with default header values */
struct ipv4_datagram ipv4_datagram_init(void);

/* Parse raw network data into an IPv4 datagram
 * Returns number of bytes parsed, or -1 on error
 * Note: Allocates memory for payload, caller must free with ipv4_datagram_free */
int ipv4_datagram_parse(struct ipv4_datagram* dgram, const void* data, size_t length);

/* Serialize an IPv4 datagram into network data
 * Returns number of bytes written, or -1 on error */
int ipv4_datagram_serialize(const struct ipv4_datagram* dgram, void* data, size_t max_length);

/* Set the payload of an IPv4 datagram
 * Makes a copy of the provided data
 * Returns 0 on success, -1 on error */
int ipv4_datagram_set_payload(struct ipv4_datagram* dgram, const void* data, size_t length);