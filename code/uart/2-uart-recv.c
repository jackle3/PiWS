// ping pong 4-byte packets back and forth.
#include "nrf-test.h"
#include "uart-to-tcp.h"

// useful to mess around with these. 
enum { ntrial = 1000, timeout_usec = 500000, nbytes = 32 };

// example possible wrapper to recv a 32-bit value.
static int net_get32(nrf_t *nic, uint8_t *out) {
    int ret = nrf_read_exact_timeout(nic, out, 32, timeout_usec);
    if(ret != 32) {
        debug("receive failed: ret=%d\n", ret);
        return 0;
    }
    return 1;
}
// example possible wrapper to send a 32-bit value.
static void net_put32(nrf_t *nic, uint32_t txaddr, uint32_t x) {
    int ret = nrf_send_noack(nic, txaddr, &x, 4);
    if(ret != 4)
        panic("ret=%d, expected 4\n");
}

void notmain(void) {
    kmalloc_init(1);
    uart_init();

    // configure server
    trace("send total=%d, %d-byte messages from server=[%x] to client=[%x]\n",
                ntrial, nbytes, server_addr, client_addr);
    

    // nrf_t *s = server_mk_noack(server_addr, nbytes);
    nrf_t *c = client_mk_noack(client_addr, nbytes);
    // run test.
    config_init_hw();
    // uint8_t buf[32];
    struct rcp_datagram dgram;
    delay_ms(4000);
    for (int i = 0; i < 10; i++) {
        net_get32(c, (uint8_t *)&dgram);
        for (int x = 0; x < dgram.header.payload_len; x++) {
            uart_put8(dgram.payload[x]);
        }
    }
    // for (int i = 0; i < 10; i++){
    //     struct rcp_datagram dgram = create_packet_hw();
    //     uint8_t buf[32];
    //     rcp_datagram_serialize(&dgram, buf, 32);
    //     nrf_send_noack(s, 0xAA, buf, 32);
    // }
    output("done!");

}
