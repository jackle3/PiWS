// Conversion from UART via laptop to rcp
#include "rcp-datagram.h"
#include "sw-uart.h"

#define mac 0xAA

/* User configurations for the header info */
void config_init(sw_uart_t u);

/* Receive user input and return a rcp packet*/
struct rcp_datagram create_packet(sw_uart_t u);

// Circular buffer used to continually parse user input
// uint8_t *buf[25];

// Predefined header for faster handling
static struct rcp_header head = {
    .payload_len = 0,
    .cksum = 0,
    .dst = 0,
    .src = mac,
    .seqno = 0,
    .flags = 0x0,
    .ackno = 0,
    .window = 0,
};