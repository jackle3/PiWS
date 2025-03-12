#pragma once

#include <stdint.h>
#include <stddef.h>

/* Type definitions */
typedef uint8_t tcp_addr_t;      /* 8-bit TCP address (0x1, 0x2, etc.) */
typedef uint32_t tcp_seqno_t;    /* 32-bit sequence number */
typedef uint16_t tcp_port_t;     /* 16-bit port number */

/* Constants */
#define TCP_MAX_PAYLOAD_SIZE 22  /* Maximum payload size (limited by RCP) */
#define TCP_MAX_WINDOW_SIZE 255  /* Maximum window size in packets */
#define TCP_DEFAULT_TIMEOUT 1000 /* Default timeout in milliseconds */
#define TCP_MAX_RETRIES 3       /* Maximum number of retransmission attempts */

/* Error codes */
#define TCP_SUCCESS 0
#define TCP_ERROR_TIMEOUT -1
#define TCP_ERROR_RETRIES -2
#define TCP_ERROR_INVALID -3
#define TCP_ERROR_MEMORY -4 