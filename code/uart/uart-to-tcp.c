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

void config_init(sw_uart_t u) {
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

struct rcp_datagram create_packet(sw_uart_t u) {
    // Create rcp struct
    kmalloc_init(1);
    struct rcp_datagram segment = { head, NULL };
    int i = 0;
    uint8_t* data = kmalloc(22);
    sw_uart_putk(&u, "Enter message, max 22 chars: \n");


    // Create the packet
    while (1) {
        // Write data
        char c = sw_uart_get8(&u);
        data[i++] = c;

        // trace("%c\n", c);

        if (i == 21 || c == '\n') {
            data[i] = '\0';
            break;
        }
    }
    sw_uart_putk(&u, "exit loop");
    // sw_uart_putk(&u, data);
    // Kmalloc for the payload, then return
    segment.header.payload_len = i + 1;
    segment.payload = data;
    sw_uart_putk(&u, "done");
    rcp_compute_checksum(&segment.header);
    sw_uart_putk(&u, "done");
    return segment;
}