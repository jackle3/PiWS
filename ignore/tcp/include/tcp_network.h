#pragma once

#include "tcp_common.h"
#include "nrf.h"
#include "rcp_datagram.h"

/* Network interface structure */
struct tcp_network {
    nrf_t *nrf;                   /* NRF device handle */
    tcp_addr_t local_addr;        /* Local TCP address */
    uint64_t nrf_addr;           /* Local NRF address */
    uint8_t pipe;                /* Local NRF pipe */
};

/* Initialize network interface */
struct tcp_network *tcp_network_init(nrf_t *nrf, 
                                   tcp_addr_t local_addr,
                                   uint64_t nrf_addr,
                                   uint8_t pipe);

/* Send a segment (RCP datagram) to a destination */
int tcp_network_send(struct tcp_network *net,
                    tcp_addr_t dst_addr,
                    const struct rcp_datagram *dgram);

/* Receive a segment (blocks until data available or timeout) */
int tcp_network_recv(struct tcp_network *net,
                    struct rcp_datagram *dgram,
                    uint32_t timeout_ms);

/* Free network interface resources */
void tcp_network_free(struct tcp_network *net); 