#ifndef RANDOM_DATA_H
#define RANDOM_DATA_H

#include <stdint.h>

#define RANDOM_DATA_SIZE (1024 * 1024)  // 1 MB

static const uint8_t random_data[RANDOM_DATA_SIZE] = {
    // Fill this with random bytes
    // For demo, let's use a simple pattern to keep file short for example:
    // In real case, generate using Python or `xxd` from /dev/urandom
    #include "random_data_bytes.inc"
};

#endif

