#include <string.h>

#include "nrf-test.h"
#include "tcp.h"

// Helper function to print TCP state
static const char* tcp_state_str(enum tcp_state state) {
    switch (state) {
        case TCP_CLOSED: return "CLOSED";
        case TCP_LISTEN: return "LISTEN";
        case TCP_SYN_SENT: return "SYN_SENT";
        case TCP_SYN_RECEIVED: return "SYN_RECEIVED";
        case TCP_ESTABLISHED: return "ESTABLISHED";
        case TCP_FIN_WAIT_1: return "FIN_WAIT_1";
        case TCP_FIN_WAIT_2: return "FIN_WAIT_2";
        case TCP_CLOSE_WAIT: return "CLOSE_WAIT";
        case TCP_LAST_ACK: return "LAST_ACK";
        case TCP_CLOSING: return "CLOSING";
        case TCP_TIME_WAIT: return "TIME_WAIT";
        default: return "UNKNOWN";
    }
}

// Test active close (client initiates the closing)
static void test_tcp_active_close(nrf_t *server_nrf, nrf_t *client_nrf) {
    trace("====== Testing Active Close (Client Initiates) ======\n");
    
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
    trace("Client state: %s, Server state: %s\n\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));

    // Send a small test message to ensure data transfer works
    trace("Sending test data...\n");
    const char *test_msg = "Test message before closing";
    size_t msg_len = strlen(test_msg);
    
    // Send from client to server
    size_t sent = tcp_send(client, test_msg, msg_len);
    assert(sent == msg_len);
    
    // Receive on server
    char buffer[100];
    size_t received = tcp_recv(server, buffer, sizeof(buffer));
    buffer[received] = '\0';
    
    trace("Server received: %s\n", buffer);
    assert(received == msg_len);
    assert(memcmp(buffer, test_msg, msg_len) == 0);
    
    // Active close: client initiates closing
    trace("\nClient initiating active close...\n");
    tcp_close(client);
    
    trace("Client state after tcp_close(): %s\n", tcp_state_str(client->state));
    assert(client->state == TCP_FIN_WAIT_1);
    
    // Process packets until server reaches CLOSE_WAIT and client reaches FIN_WAIT_2
    trace("\nProcessing connection until client in FIN_WAIT_2 and server in CLOSE_WAIT...\n");
    uint32_t timeout = timer_get_usec() + 5000000; // 5 second timeout
    
    while ((client->state != TCP_FIN_WAIT_2 || server->state != TCP_CLOSE_WAIT) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nAfter first half-close:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_FIN_WAIT_2);
    assert(server->state == TCP_CLOSE_WAIT);
    
    // Server closes its side
    trace("\nServer calling tcp_close()...\n");
    tcp_close(server);
    
    trace("Server state after tcp_close(): %s\n", tcp_state_str(server->state));
    assert(server->state == TCP_LAST_ACK);
    
    // Process until client reaches TIME_WAIT and server reaches CLOSED
    trace("\nProcessing connection until client in TIME_WAIT and server in CLOSED...\n");
    timeout = timer_get_usec() + 5000000; // 5 second timeout
    
    while ((client->state != TCP_TIME_WAIT || server->state != TCP_CLOSED) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nAfter second half-close:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_TIME_WAIT);
    assert(server->state == TCP_CLOSED);
    
    // Process until client reaches CLOSED due to TIME_WAIT timeout
    // We'll artificially accelerate this by directly setting fin_time
    trace("\nAccelerating TIME_WAIT timeout...\n");
    client->fin_time = 0; // This will make the 2*MSL timeout check pass immediately
    
    timeout = timer_get_usec() + 1000000; // 1 second timeout
    
    while (client->state != TCP_CLOSED && timer_get_usec() < timeout) {
        tcp_process(client);
        trace("Client: %s\n", tcp_state_str(client->state));
    }
    
    trace("\nFinal states:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_CLOSED);
    assert(server->state == TCP_CLOSED);
    
    trace("\n====== Active Close Test Passed! ======\n\n");
}

// Test passive close (server initiates the closing)
static void test_tcp_passive_close(nrf_t *server_nrf, nrf_t *client_nrf) {
    trace("====== Testing Passive Close (Server Initiates) ======\n");
    
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
    trace("Client state: %s, Server state: %s\n\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));

    // Server initiates closing
    trace("Server initiating close...\n");
    tcp_close(server);
    
    trace("Server state after tcp_close(): %s\n", tcp_state_str(server->state));
    assert(server->state == TCP_FIN_WAIT_1);
    
    // Process packets until client reaches CLOSE_WAIT and server reaches FIN_WAIT_2
    trace("\nProcessing connection until server in FIN_WAIT_2 and client in CLOSE_WAIT...\n");
    uint32_t timeout = timer_get_usec() + 5000000; // 5 second timeout
    
    while ((server->state != TCP_FIN_WAIT_2 || client->state != TCP_CLOSE_WAIT) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nAfter first half-close:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(server->state == TCP_FIN_WAIT_2);
    assert(client->state == TCP_CLOSE_WAIT);
    
    // Client closes its side
    trace("\nClient calling tcp_close()...\n");
    tcp_close(client);
    
    trace("Client state after tcp_close(): %s\n", tcp_state_str(client->state));
    assert(client->state == TCP_LAST_ACK);
    
    // Process until server reaches TIME_WAIT and client reaches CLOSED
    trace("\nProcessing connection until server in TIME_WAIT and client in CLOSED...\n");
    timeout = timer_get_usec() + 5000000; // 5 second timeout
    
    while ((server->state != TCP_TIME_WAIT || client->state != TCP_CLOSED) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nAfter second half-close:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(server->state == TCP_TIME_WAIT);
    assert(client->state == TCP_CLOSED);
    
    // Process until server reaches CLOSED due to TIME_WAIT timeout
    // We'll artificially accelerate this by directly setting fin_time
    trace("\nAccelerating TIME_WAIT timeout...\n");
    server->fin_time = 0; // This will make the 2*MSL timeout check pass immediately
    
    timeout = timer_get_usec() + 1000000; // 1 second timeout
    
    while (server->state != TCP_CLOSED && timer_get_usec() < timeout) {
        tcp_process(server);
        trace("Server: %s\n", tcp_state_str(server->state));
    }
    
    trace("\nFinal states:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_CLOSED);
    assert(server->state == TCP_CLOSED);
    
    trace("\n====== Passive Close Test Passed! ======\n\n");
}

// Test simultaneous close
static void test_tcp_simultaneous_close(nrf_t *server_nrf, nrf_t *client_nrf) {
    trace("====== Testing Simultaneous Close ======\n");
    
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
    trace("Client state: %s, Server state: %s\n\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));

    // Both sides initiate close simultaneously
    trace("Both sides initiating close simultaneously...\n");
    tcp_close(client);
    tcp_close(server);
    
    trace("After tcp_close():\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_FIN_WAIT_1);
    assert(server->state == TCP_FIN_WAIT_1);
    
    // Process packets until both sides reach CLOSING state
    trace("\nProcessing connection until both sides reach CLOSING state...\n");
    uint32_t timeout = timer_get_usec() + 5000000; // 5 second timeout
    
    while ((client->state != TCP_CLOSING || server->state != TCP_CLOSING) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nAfter exchanging FINs:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    // Process until both sides reach TIME_WAIT
    trace("\nProcessing connection until both sides reach TIME_WAIT...\n");
    timeout = timer_get_usec() + 5000000; // 5 second timeout
    
    while ((client->state != TCP_TIME_WAIT || server->state != TCP_TIME_WAIT) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nAfter exchanging ACKs for FINs:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_TIME_WAIT);
    assert(server->state == TCP_TIME_WAIT);
    
    // Accelerate TIME_WAIT timeout
    trace("\nAccelerating TIME_WAIT timeout...\n");
    client->fin_time = 0;
    server->fin_time = 0;
    
    // Process until both sides reach CLOSED
    timeout = timer_get_usec() + 1000000; // 1 second timeout
    
    while ((client->state != TCP_CLOSED || server->state != TCP_CLOSED) 
            && timer_get_usec() < timeout) {
        tcp_process(client);
        tcp_process(server);
        trace("Client: %s, Server: %s\n", 
            tcp_state_str(client->state), tcp_state_str(server->state));
    }
    
    trace("\nFinal states:\n");
    trace("Client state: %s, Server state: %s\n", 
          tcp_state_str(client->state), tcp_state_str(server->state));
          
    assert(client->state == TCP_CLOSED);
    assert(server->state == TCP_CLOSED);
    
    trace("\n====== Simultaneous Close Test Passed! ======\n\n");
}

void notmain(void) {
    kmalloc_init(64);

    trace("configuring no-ack server=[%x] with %d nbyte msgs\n", server_addr, RCP_TOTAL_SIZE);
    nrf_t *s = server_mk_noack(server_addr, RCP_TOTAL_SIZE);

    trace("configuring no-ack client=[%x] with %d nbyte msg\n", client_addr, RCP_TOTAL_SIZE);
    nrf_t *c = client_mk_noack(client_addr, RCP_TOTAL_SIZE);

    // Check compatibility
    if (!nrf_compat(c, s))
        panic("did not configure correctly: not compatible\n");

    // Reset stats
    nrf_stat_start(s);
    nrf_stat_start(c);

    trace("Starting tests...\n\n");

    // Run all closing tests
    test_tcp_active_close(s, c);
    test_tcp_passive_close(s, c);
    test_tcp_simultaneous_close(s, c);

    trace("\nAll TCP closing tests passed successfully!\n");

    // Print stats
    nrf_stat_print(s, "server: done with tests");
    nrf_stat_print(c, "client: done with tests");
} 