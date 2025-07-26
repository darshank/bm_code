#include <stdio.h>
#include <string.h>
#include "lz4.h"
#include "random_data.h"  // Contains 1MB data array as in previous example

#define COMPRESSED_BUFFER_SIZE (RANDOM_DATA_SIZE + (RANDOM_DATA_SIZE / 255) + 16)

static unsigned char compressed_data[COMPRESSED_BUFFER_SIZE];
static unsigned char decompressed_data[RANDOM_DATA_SIZE];

int main(void) {
    printf("Original size: %d bytes\n", RANDOM_DATA_SIZE);

    // Compress
    int compressed_size = LZ4_compress_default((const char*)random_data,
                                               (char*)compressed_data,
                                               RANDOM_DATA_SIZE,
                                               COMPRESSED_BUFFER_SIZE);

    if (compressed_size <= 0) {
        printf("Compression failed\n");
        return 1;
    }

    printf("Compressed size: %d bytes (%.2f%%)\n", compressed_size,
           (compressed_size * 100.0) / RANDOM_DATA_SIZE);

    // Decompress
    int decompressed_size = LZ4_decompress_safe((const char*)compressed_data,
                                                (char*)decompressed_data,
                                                compressed_size,
                                                RANDOM_DATA_SIZE);

    if (decompressed_size < 0) {
        printf("Decompression failed\n");
        return 1;
    }

    printf("Decompressed size: %d bytes\n", decompressed_size);

    // Verify
    if (decompressed_size == RANDOM_DATA_SIZE &&
        memcmp(random_data, decompressed_data, RANDOM_DATA_SIZE) == 0) {
        printf("Verification PASSED\n");
    } else {
        printf("Verification FAILED\n");
    }

    return 0;
}

