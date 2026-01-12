// tests/ffi/packed_helper.c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

// Must match the Virex struct definition
struct Data {
    uint8_t a;
    uint32_t b;
    uint8_t c;
} __attribute__((packed));

int verify_struct_size(int expected_size) {
    if (sizeof(struct Data) == expected_size) {
        return 1;
    }
    printf("C Side Error: sizeof(struct Data) is %lu, expected %d\n", sizeof(struct Data), expected_size);
    return 0;
}

int verify_packed_struct(struct Data* d) {
    if (d->a != 255) {
        printf("C Side Error: d->a expected 255, got %d\n", d->a);
        return 0;
    }
    if (d->b != 0xAABBCCDD) {
        printf("C Side Error: d->b expected 0xAABBCCDD, got 0x%X\n", d->b);
        return 0;
    }
    if (d->c != 127) {
        printf("C Side Error: d->c expected 127, got %d\n", d->c);
        return 0;
    }
    
    // Verify offsets
    if (offsetof(struct Data, b) != 1) {
        printf("C Side Error: offsetof(b) expected 1, got %lu\n", offsetof(struct Data, b));
        return 0;
    }
    
    return 1;
}
