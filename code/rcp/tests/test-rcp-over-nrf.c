#include "nrf.h"
#include "nrf-test.h"
#include "rcp-datagram.h"
#include <string.h>

// useful to mess around with these.
enum { ntrial = 1000, timeout_usec = 1000 };

// max message size.
typedef struct {
    uint8_t data[RCP_TOTAL_SIZE];
} data_t;
_Static_assert(sizeof(data_t) == RCP_TOTAL_SIZE, "invalid size");

static void test_rcp_packet(nrf_t *server, nrf_t *client, int verbose_p) {
    unsigned client_addr = client->rxaddr;
    unsigned ntimeout = 0, npackets = 0;

    for(unsigned i = 0; i < ntrial; i++) {
        if(verbose_p && i && i % 100 == 0)
            trace("sent %d ack'd packets\n", i);

        // Create RCP datagram with test message
        struct rcp_datagram dgram = rcp_datagram_init();
        char test_msg[32];
        snprintk(test_msg, sizeof(test_msg), "test message %d", i);
        
        // Print message about to be sent
        if (i % 100 == 0) {
            trace("Sending message: %s\n", test_msg);
        }
        
        // Set up header fields
        dgram.header.src = server->rxaddr;
        dgram.header.dst = client_addr;
        dgram.header.seqno = i;
        dgram.header.window = 1;
        rcp_set_flag(&dgram.header, RCP_FLAG_SYN);
        
        // Set payload and compute checksum
        if(rcp_datagram_set_payload(&dgram, test_msg, strlen(test_msg) + 1) < 0) {
            panic("Failed to set payload\n");
        }
        rcp_compute_checksum(&dgram.header);

        // Serialize into data_t struct
        data_t d = {0};
        int packet_len = rcp_datagram_serialize(&dgram, d.data, RCP_TOTAL_SIZE);
        if(packet_len < 0) {
            panic("Failed to serialize packet\n");
        }

        // Send packet
        int ret = nrf_send_ack(server, client_addr, &d, RCP_TOTAL_SIZE);
        if(ret != RCP_TOTAL_SIZE) {
            panic("send failed\n");
        }

        // Receive packet
        data_t rx;
        ret = nrf_read_exact_timeout(client, &rx, RCP_TOTAL_SIZE, timeout_usec);
        if(ret == RCP_TOTAL_SIZE) {
            // Parse received packet
            struct rcp_datagram rx_dgram = rcp_datagram_init();
            if(rcp_datagram_parse(&rx_dgram, rx.data, RCP_TOTAL_SIZE) < 0) {
                nrf_output("client: corrupt packet=%d\n", i);
                continue;
            }

            // Print received message
            if (i % 100 == 0) {
                trace("Received message: %s\n", (char*)rx_dgram.payload);
            }

            // Verify received data matches
            if(memcmp(rx_dgram.payload, test_msg, rx_dgram.header.payload_len) == 0) {
                npackets++;
            } else {
                nrf_output("client: data mismatch packet=%d\n", i);
            }
        } else {
            if(verbose_p)
                output("receive failed for packet=%d, nbytes=%d ret=%d\n", i, RCP_TOTAL_SIZE, ret);
            ntimeout++;
        }
    }

    trace("trial: total successfully sent %d ack'd packets lost [%d]\n",
        npackets, ntimeout);
    assert((ntimeout + npackets) == ntrial);
}

void notmain(void) {
    kmalloc_init(64);

    trace("configuring reliable (acked) server=[%x] with RCP packets\n", server_addr);

    nrf_t *s = server_mk_ack(server_addr, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_ack(client_addr, RCP_TOTAL_SIZE);

    nrf_stat_start(s);
    nrf_stat_start(c);

    // run test.
    test_rcp_packet(s, c, 1);

    // emit all the stats.
    nrf_stat_print(s, "server: done with RCP test");
    nrf_stat_print(c, "client: done with RCP test");
}