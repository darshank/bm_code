#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"
#include "random_data.h"

int main() {
    printf("Original Data Size: %d bytes\n", RANDOM_DATA_SIZE);

    // Allocate buffer for compressed data (slightly bigger than input)
    mz_ulong max_compressed_size = mz_compressBound(RANDOM_DATA_SIZE);
    unsigned char *compressed_data = malloc(max_compressed_size);
    if (!compressed_data) {
        fprintf(stderr, "Memory allocation failed for compressed buffer\n");
        return 1;
    }

    // Compress
    mz_ulong compressed_size = max_compressed_size;
    int status = mz_compress(compressed_data, &compressed_size,
                              random_data, RANDOM_DATA_SIZE);
    if (status != MZ_OK) {
        fprintf(stderr, "Compression failed: %d\n", status);
        free(compressed_data);
        return 1;
    }

    printf("Compressed size: %lu bytes (%.2f%%)\n", compressed_size,
           (compressed_size * 100.0) / RANDOM_DATA_SIZE);

    // Allocate buffer for decompression
    unsigned char *decompressed_data = malloc(RANDOM_DATA_SIZE);
    if (!decompressed_data) {
        fprintf(stderr, "Memory allocation failed for decompression\n");
        free(compressed_data);
        return 1;
    }

    // Decompress
    mz_ulong decompressed_size = RANDOM_DATA_SIZE;
    status = mz_uncompress(decompressed_data, &decompressed_size,
                            compressed_data, compressed_size);
    if (status != MZ_OK) {
        fprintf(stderr, "Decompression failed: %d\n", status);
        free(compressed_data);
        free(decompressed_data);
        return 1;
    }

    printf("Decompressed size: %lu bytes\n", decompressed_size);

    // Verify
    if (decompressed_size == RANDOM_DATA_SIZE &&
        memcmp(random_data, decompressed_data, RANDOM_DATA_SIZE) == 0) {
        printf("Verification PASSED: Data matches original\n");
    } else {
        printf("Verification FAILED: Data mismatch\n");
    }

    free(compressed_data);
    free(decompressed_data);
    return 0;
}

