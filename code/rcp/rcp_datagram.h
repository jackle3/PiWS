#pragma once

#include "rcp_header.h"
#include <stddef.h>
#include <stdint.h>

/* RCP Datagram structure
 * Contains both the header and payload of an RCP packet
 */
struct rcp_datagram {
    struct rcp_header header;  /* RCP header */
    uint8_t* payload;         /* Pointer to payload data */
};

/* Initialize a new RCP datagram with default header values */
struct rcp_datagram rcp_datagram_init(void);

/* Parse raw network data into an RCP datagram
 * Returns number of bytes parsed, or -1 on error
 * Note: Allocates memory for payload, caller must free with rcp_datagram_free */
int rcp_datagram_parse(struct rcp_datagram* dgram, const void* data, size_t length);

/* Serialize an RCP datagram into network data
 * Returns number of bytes written, or -1 on error */
int rcp_datagram_serialize(const struct rcp_datagram* dgram, void* data, size_t max_length);

/* Set the payload of an RCP datagram
 * Makes a copy of the provided data
 * Returns 0 on success, -1 on error */
int rcp_datagram_set_payload(struct rcp_datagram* dgram, const void* data, size_t length);