#include <string.h>

#include "nrf-test.h"
#include "tcp.h"
#include "uart-to-tcp.h"

#define PACKET_SIZE 32

// routing table array
static uint32_t routing_table[256] = {0, server_addr, server_addr_2};

// busy loop, and check nrf_read_exact_timeout, if we have a packet then parse the packet
// as a datagram and route it to correct destination based on the destination address
void route_messages(nrf_t *server, nrf_t *client)
{
    uint8_t buffer[PACKET_SIZE];
    struct rcp_datagram dgram;

    while (1)
    {
        int bytes_read = nrf_read_exact_timeout(server, buffer, PACKET_SIZE, 1000);
        if (bytes_read == PACKET_SIZE)
        {
            // Parse the packet as an RCP datagram
            if (rcp_datagram_parse(&dgram, buffer, bytes_read) != -1)
            {
                // Route it to the correct destination based on the destination address
                uint32_t rcp_dst = routing_table[dgram.header.dst];

                nrf_send_noack(client, rcp_dst, buffer, bytes_read);
                trace("Route message to %x\n", rcp_dst);
            }
            else
            {
                trace("Failed to parse RCP datagram\n");
            }
        }
    }
}

void notmain(void)
{
    kmalloc_init(64);
    uart_init();

    trace("configuring no-ack server=[%x] with %d nbyte msgs\n", router_server_addr, RCP_TOTAL_SIZE);
    nrf_t *s = router_mk_noack(router_server_addr, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable server config:\n", s);

    trace("configuring no-ack client=[%x] with %d nbyte msg\n", router_client_addr, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_noack(router_client_addr, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable client config:\n", c);

    // Check compatibility
    if (!nrf_compat(c, s))
        panic("did not configure correctly: not compatible\n");

    // Reset stats
    nrf_stat_start(s);
    nrf_stat_start(c);

    // trace("Starting test...\n");

    route_messages(s, c);

    // Print stats
    nrf_stat_print(s, "server: done with test");
    nrf_stat_print(c, "client: done with test");
}