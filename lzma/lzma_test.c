#include <stdio.h>
#include <string.h>
#include "LzmaEnc.h"
#include "LzmaDec.h"
#include "random_data.h"

#define OUT_BUF_SIZE (RANDOM_DATA_SIZE + RANDOM_DATA_SIZE / 3 + 128)  // estimated
#define PROPS_SIZE 5

static unsigned char compressed_data[OUT_BUF_SIZE];
static unsigned char decompressed_data[RANDOM_DATA_SIZE];
static unsigned char props[PROPS_SIZE];

extern void my_reset_pool(void) ;
int compression_levels =10;
int outer_loop = 4;
int main() {
    printf("Original size: %d bytes\n", RANDOM_DATA_SIZE);
    int level_idx = 0;
    int loop_idx = 0;
    for(loop_idx = 0; loop_idx < outer_loop ; loop_idx++)
    {
	    for(level_idx=0; level_idx < compression_levels; level_idx++)
	    {
			printf("[%d] Compression Level %d \n", loop_idx, level_idx);
			my_reset_pool();
    			size_t props_size = PROPS_SIZE;
		    size_t dest_len = sizeof(compressed_data);

		    // Compress
		    int res = LzmaCompress(compressed_data, &dest_len,
				    random_data, RANDOM_DATA_SIZE,
				    props, &props_size,
				    level_idx,         // compression level (0-9)
				    1 << 20,   // dictionary size = 1MB
				    3, 0, 2, 32, 1);

		    if (res != SZ_OK) {
			    printf("Compression failed: %d\n", res);
			    return 1;
		    }

		    printf("Compressed size: %zu bytes (%.2f%%)\n", dest_len,
				    (dest_len * 100.0) / RANDOM_DATA_SIZE);

		    // Decompress
		    SizeT dest_len_dec = RANDOM_DATA_SIZE;
		    SizeT src_len = dest_len;

		    ELzmaStatus status;
		    res = LzmaUncompress(decompressed_data, &dest_len_dec,
				    compressed_data, &src_len,
				    props, props_size);

		    if (res != SZ_OK || dest_len_dec != RANDOM_DATA_SIZE) {
			    printf("Decompression failed: %d\n", res);
			    return 1;
		    }

		    // Verify
		    if (memcmp(random_data, decompressed_data, RANDOM_DATA_SIZE) == 0) {
			    printf("Verification PASSED\n");
		    } else {
			    printf("Verification FAILED\n");
		    }
	    } //level_idx
    } //outer_loop

    return 0;
}

