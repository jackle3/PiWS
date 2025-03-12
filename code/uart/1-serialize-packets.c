// test that tcp packets are created properly.
#include "rpi.h"
#include "uart-to-tcp.h"

void notmain(void) {
    output("Starting UART-to-TCP test...\n");
    // hw_uart_disable();
    kmalloc_init(1);

    // Initialize software UART on GPIO pins 14 (TX) and 15 (RX)
    // sw_uart_t u = sw_uart_init(14, 15, 115200);
    uart_init();

    // Configure header with user input
    config_init_hw();

    // Create an RCP packet from UART input
    struct rcp_datagram packet = create_packet_hw();

    // Reset to using hardware UART

    // Print out the resulting TCP packet details
    printk("\n--- Generated TCP Packet Details ---\n");
    printk("Payload Length: %d\n", packet.header.payload_len);
    printk("Checksum: %d\n", packet.header.cksum);
    printk("Destination Address: %d\n", packet.header.dst);
    printk("Source Address: %d\n", packet.header.src);
    printk("Sequence Number: %d\n", packet.header.seqno);
    printk("Flags: %d\n", packet.header.flags);
    printk("Acknowledgment Number: %d\n", packet.header.ackno);
    printk("Window Size: %d\n", packet.header.window);

    printk("Payload Data: ");
    for (int i = 0; i < packet.header.payload_len; i++) {
        printk("%c", packet.payload[i]);
    }
    printk("\n");
    printk("\nTest completed.\n");
}