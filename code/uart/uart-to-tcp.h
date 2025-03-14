// Conversion from UART via laptop to rcp
#include "rcp-datagram.h"
#include "sw-uart.h"

#define mac 0xAA

/* User configurations for the header info */
void config_init_sw(sw_uart_t u);
uint8_t config_init_hw();

/* Receive user input and return a rcp packet*/
struct rcp_datagram create_packet_sw(sw_uart_t u);
struct rcp_datagram create_packet_hw();

// Circular buffer used to continually parse user input
// uint8_t *buf[25];

void uart_putk(char *s);

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