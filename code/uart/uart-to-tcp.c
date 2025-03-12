// Test printing out our RCP packet correctly from user input
#include "rpi.h"
#include "uart-to-tcp.h"

// Mini helper atoi
uint8_t my_atoi(const char *str) {
    uint8_t result = 0;
    
    // Skip leading whitespaces
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }
    
    // Convert characters to integer
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result;
}

// BAD THIS SHOULDN"T BE HERE BUT IT WORKS
// must figure out how to put in uart.c
void uart_putk(char *s) {
    for(; *s; s++)
        uart_put8(*s);
}

void config_init_sw(sw_uart_t u) {
    sw_uart_putk(&u, "Please enter dst (0-255)\n");
    char buf[4] = {0};

    // Larger inputs should be ignored
    for (int i = 0; i < 4; i++) {
        char c = sw_uart_get8(&u);
        if (c == '\n') {
            buf[i] = '\0';
            break;
        }
        buf[i] = c;
        // Values greater than 255 wrap around, that's on the user
    }    
    // Set dst to user input
    head.dst = my_atoi(buf);
}

void config_init_hw() {
    uart_putk("Please enter dst (0-255)\n");
    char buf[4] = {0};

    // Larger inputs should be ignored
    for (int i = 0; i < 4; i++) {
        char c = uart_get8();
        if (c == '\n') {
            buf[i] = '\0';
            break;
        }
        buf[i] = c;
        // Values greater than 255 wrap around, that's on the user
    }    
    // Set dst to user input
    head.dst = my_atoi(buf);
}

struct rcp_datagram create_packet_sw(sw_uart_t u) {
    // Create rcp struct
    struct rcp_datagram segment = { head, NULL };
    int i = 0;
    uint8_t* data = kmalloc(220);
    sw_uart_putk(&u, "Enter message, max 220 chars: \n");


    // Create the packet
    while (1) {
        // Write data
        char c = sw_uart_get8(&u);
        data[i++] = c;

        if (i == 219 || c == '\n') {
            data[i] = '\0';
            break;
        }
    }

    // Serialize packets:
    int num_packets = (i - 1) / 22;
    int remaining = (i - 1) % 22;
    for (int j = 0; j < num_packets; j++) {
        rcp_datagram_set_payload(&segment, &data[j * 22], 22);
        rcp_compute_checksum(&segment.header);
        // TODO replace printout with enqueue
        // printk("cksum: %d\n", segment.header.cksum);
        for (int x = 0; x < segment.header.payload_len; x++) {
            sw_uart_put8(&u, segment.payload[x]);
        }
        sw_uart_putk(&u, "\n");
    }
    if (remaining) {
        rcp_datagram_set_payload(&segment, &data[num_packets * 22], remaining);
        rcp_compute_checksum(&segment.header);
        // TODO replace with enqueue, final segment should be covered by test
        // printk("cksum: %d\n", segment.header.cksum);
        // for (int x = 0; x < segment.header.payload_len; x++) {
        //     sw_uart_put8(&u, segment.payload[i]);
        // }
        // sw_uart_put8(&u, '\n');
    }    
    
    /*
    segment.header.payload_len = i + 1;
    segment.payload = data;
    rcp_compute_checksum(&segment.header);
    */
    return segment;
}

struct rcp_datagram create_packet_hw() {
    // Create rcp struct
    struct rcp_datagram segment = { head, NULL };
    int i = 0;
    uint8_t* data = kmalloc(220);
    uart_putk("Enter message, max 220 chars: \n");


    // Create the packet
    while (1) {
        // Write data
        char c = uart_get8();
        data[i++] = c;

        if (i == 219 || c == '\n') {
            data[i] = '\0';
            break;
        }
    }

    // Serialize packets:
    int num_packets = (i - 1) / 22;
    int remaining = (i - 1) % 22;
    for (int j = 0; j < num_packets; j++) {
        rcp_datagram_set_payload(&segment, &data[j * 22], 22);
        rcp_compute_checksum(&segment.header);
        // TODO replace printout with enqueue
        for (int x = 0; x < segment.header.payload_len; x++) {
            uart_put8(segment.payload[x]);
        }
        uart_putk("\n");
    }
    if (remaining) {
        rcp_datagram_set_payload(&segment, &data[num_packets * 22], remaining);
        rcp_compute_checksum(&segment.header);
        // todo enqueue, also printed out by test
    }    
    
    return segment;
}