#include <stdio.h>
#include <string.h>

#include "bytestream.h"

// Test basic bytestream operations
static void test_bytestream(void) {
    printk("--------------------------------\n");
    printk("Starting bytestream test...\n");

    bytestream_t bs = bs_init();
    printk("Bytestream initialized with capacity %d\n", BS_CAPACITY);

    const char *test_data = "Hello, TCP!";
    size_t len = strlen(test_data);

    // Test writing
    size_t written = bs_write(&bs, (uint8_t *)test_data, len);
    assert(written == len);
    assert(bs_bytes_available(&bs) == len);
    assert(bs_bytes_written(&bs) == len);
    printk("Successfully wrote %u bytes: '%s'\n", written, test_data);

    // Test peeking
    uint8_t peek_buffer[20];
    size_t peeked = bs_peek(&bs, peek_buffer, sizeof(peek_buffer));
    assert(peeked == len);
    assert(memcmp(peek_buffer, test_data, len) == 0);
    assert(bs_bytes_available(&bs) == len);  // Peek doesn't consume data
    peek_buffer[peeked] = '\0';
    printk("Successfully peeked %u bytes: '%s'\n", peeked, peek_buffer);

    // Test reading
    uint8_t read_buffer[20];
    size_t read = bs_read(&bs, read_buffer, sizeof(read_buffer));
    assert(read == len);
    assert(memcmp(read_buffer, test_data, len) == 0);
    assert(bs_bytes_available(&bs) == 0);
    assert(bs_bytes_popped(&bs) == len);
    read_buffer[read] = '\0';
    printk("Successfully read %u bytes: '%s'\n", read, read_buffer);

    // Test end of input
    assert(!bs_reader_finished(&bs));  // Not finished yet
    bs_end_input(&bs);
    assert(bs_reader_finished(&bs));  // Now finished (EOF + no data)
    printk("Successfully tested end of input\n");

    // Test remaining capacity
    assert(bs_remaining_capacity(&bs) == BS_CAPACITY);
    printk("Remaining capacity: %u\n", BS_CAPACITY);

    printk("Bytestream test passed!\n");
    printk("--------------------------------\n");
}

void notmain(void) {
    printk("Starting TCP implementation tests...\n\n");
    kmalloc_init(64);
    printk("Memory initialized\n");

    test_bytestream();

    printk("\nBytestream test passed!\n");
}