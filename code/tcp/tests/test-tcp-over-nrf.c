#include "tcp.h"
#include "nrf-test.h"
#include <string.h>

static void test_tcp_reliable_delivery(nrf_t *server_nrf, nrf_t *client_nrf) {
    // Create TCP connections
    trace("Creating TCP connections...\n");
    struct tcp_connection *server = tcp_init(server_nrf, client_nrf->rxaddr, true);
    struct tcp_connection *client = tcp_init(client_nrf, server_nrf->rxaddr, false);
    
    // Handle handshake
    trace("Handshaking...\n");
    while (server->state != TCP_ESTABLISHED || client->state != TCP_ESTABLISHED) {
        tcp_do_handshake(server);
        tcp_do_handshake(client);
    }

    trace("Connection established!\n");
    
    // Send test data
    trace("Sending test data...\n");
    const char *test_msg = "Test TCP message";
    int ret = tcp_send(client, test_msg, strlen(test_msg));
    assert(ret == strlen(test_msg));

    // Receive data
    trace("Receiving data...\n");
    char buffer[100];
    ret = tcp_recv(server, buffer, sizeof(buffer));
    assert(ret == strlen(test_msg));
    assert(memcmp(buffer, test_msg, strlen(test_msg)) == 0);

    // Clean up
    trace("Closing connections...\n");
    tcp_close(client);
    tcp_close(server);
}

void notmain(void) {
    kmalloc_init(64);
    
    trace("configuring no-ack server=[%x] with %d nbyte msgs\n", 
                server_addr, RCP_TOTAL_SIZE);
    nrf_t *s = server_mk_ack(server_addr, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable server config:\n", s);

    trace("configuring no-ack client=[%x] with %d nbyte msg\n", 
                client_addr, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_ack(client_addr, RCP_TOTAL_SIZE);
    // nrf_dump("unreliable client config:\n", c);

    // Check compatibility
    if(!nrf_compat(c, s))
        panic("did not configure correctly: not compatible\n");

    // Reset stats
    nrf_stat_start(s);
    nrf_stat_start(c);

    trace("Starting test...\n");

    test_tcp_reliable_delivery(s, c);

    // Print stats
    nrf_stat_print(s, "server: done with test");
    nrf_stat_print(c, "client: done with test");
}