// test that tcp packets are created properly.
#include "rpi.h"
#include "uart-to-tcp.h"

void notmain(void) {
    output("Starting UART-to-TCP test...\n");
    hw_uart_disable();

    // Initialize software UART on GPIO pins 14 (TX) and 15 (RX)
    sw_uart_t u = sw_uart_init(14, 15, 115200);

    // Configure header with user input
    config_init(u);

    // Create an RCP packet from UART input
    struct rcp_datagram packet = create_packet(u);

    // Reset to using hardware UART
    uart_init();

    // Print out the resulting TCP packet details
    trace("\n--- Generated TCP Packet Details ---\n");
    trace("Payload Length: %d\n", packet.header.payload_len);
    trace("Checksum: %d\n", packet.header.cksum);
    trace("Destination Address: %d\n", packet.header.dst);
    trace("Source Address: %d\n", packet.header.src);
    trace("Sequence Number: %d\n", packet.header.seqno);
    trace("Flags: %d\n", packet.header.flags);
    trace("Acknowledgment Number: %d\n", packet.header.ackno);
    trace("Window Size: %d\n", packet.header.window);

    trace("Payload Data: ");
    for (int i = 0; i < packet.header.payload_len; i++) {
        printk("%c", packet.payload[i]);
    }
    trace("\n");
    trace("\nTest completed.\n");
}