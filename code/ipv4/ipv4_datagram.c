#include "ipv4_datagram.h"
#include <stdlib.h>
#include <string.h>

struct ipv4_datagram ipv4_datagram_init(void) {
    struct ipv4_datagram dgram = {
        .header = ipv4_init(),
        .payload = NULL,
        .payload_length = 0
    };
    return dgram;
}

int ipv4_datagram_parse(struct ipv4_datagram* dgram, const void* data, size_t length) {
    if (!dgram || !data || length < IPV4_HEADER_LENGTH) {
        return -1;
    }

    // Parse header first
    ipv4_parse(&dgram->header, data);
    
    // Calculate payload length from header's total length
    size_t payload_len = ipv4_payload_length(&dgram->header);
    if (length < IPV4_HEADER_LENGTH + payload_len) {
        return -1;  // Not enough data
    }

    // Allocate and copy payload
    dgram->payload = kmalloc(payload_len);
    if (!dgram->payload) {
        return -1;  // Memory allocation failed
    }
    
    memcpy(dgram->payload, 
           (const uint8_t*)data + IPV4_HEADER_LENGTH, 
           payload_len);
    dgram->payload_length = payload_len;

    return IPV4_HEADER_LENGTH + payload_len;
}

int ipv4_datagram_serialize(const struct ipv4_datagram* dgram, void* data, size_t max_length) {
    if (!dgram || !data) {
        return -1;
    }

    size_t total_length = IPV4_HEADER_LENGTH + dgram->payload_length;
    if (max_length < total_length) {
        return -1;  // Buffer too small
    }

    // Serialize header
    ipv4_serialize(&dgram->header, data);

    // Copy payload
    if (dgram->payload && dgram->payload_length > 0) {
        memcpy((uint8_t*)data + IPV4_HEADER_LENGTH, 
               dgram->payload, 
               dgram->payload_length);
    }

    return total_length;
}

int ipv4_datagram_set_payload(struct ipv4_datagram* dgram, const void* data, size_t length) {
    if (!dgram) {
        return -1;
    }

    // Free existing payload if any
    dgram->payload = NULL;
    dgram->payload_length = 0;

    if (data && length > 0) {
        // Allocate and copy new payload
        dgram->payload = kmalloc(length);
        if (!dgram->payload) {
            return -1;  // Memory allocation failed
        }
        
        memcpy(dgram->payload, data, length);
        dgram->payload_length = length;

        // Update header length field
        dgram->header.len = IPV4_HEADER_LENGTH + length;
    }

    return 0;
}