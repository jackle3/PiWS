#include "rcp_datagram.h"
#include <string.h>

struct rcp_datagram rcp_datagram_init(void) {
    struct rcp_datagram dgram = {
        .header = rcp_header_init(),
        .payload = NULL,
    };
    return dgram;
}

int rcp_datagram_parse(struct rcp_datagram* dgram, const void* data, size_t length) {
    if (!dgram || !data || length < RCP_HEADER_LENGTH) {
        return -1;
    }

    // Parse header first
    rcp_parse(&dgram->header, data);
    
    // Get payload length from header
    size_t payload_len = dgram->header.payload_len;
    if (payload_len > RCP_MAX_PAYLOAD || length < RCP_HEADER_LENGTH + payload_len) {
        return -1;  // Invalid length or not enough data
    }

    // Allocate and copy payload if present
    if (payload_len > 0) {
        dgram->payload = kmalloc(payload_len);
        if (!dgram->payload) {
            return -1;  // Memory allocation failed
        }
        
        memcpy(dgram->payload, 
               (const uint8_t*)data + RCP_HEADER_LENGTH, 
               payload_len);
        dgram->header.payload_len = payload_len;
    }

    return RCP_HEADER_LENGTH + payload_len;
}

int rcp_datagram_serialize(const struct rcp_datagram* dgram, void* data, size_t max_length) {
    if (!dgram || !data || max_length < RCP_HEADER_LENGTH) {
        return -1;
    }

    size_t total_length = RCP_HEADER_LENGTH + dgram->header.payload_len;
    if (max_length < total_length || total_length > RCP_TOTAL_SIZE) {
        return -1;  // Buffer too small or packet too large
    }

    // Serialize header
    rcp_serialize(&dgram->header, data);

    // Copy payload if present
    if (dgram->payload && dgram->header.payload_len > 0) {
        memcpy((uint8_t*)data + RCP_HEADER_LENGTH, 
               dgram->payload, 
               dgram->header.payload_len);
    }

    return total_length;
}

int rcp_datagram_set_payload(struct rcp_datagram* dgram, const void* data, size_t length) {
    if (!dgram || length > RCP_MAX_PAYLOAD) {
        return -1;
    }

    // Free existing payload if any
    dgram->payload = NULL;
    dgram->header.payload_len = 0;

    if (data && length > 0) {
        // Allocate and copy new payload
        dgram->payload = kmalloc(length);
        if (!dgram->payload) {
            return -1;  // Memory allocation failed
        }
        
        memcpy(dgram->payload, data, length);
        dgram->header.payload_len = length;
    }

    return 0;
}