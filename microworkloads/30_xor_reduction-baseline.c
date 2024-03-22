#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "baseline.h"

static uint8_t xor_reduction8(uint8_t x) {
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return x & 1;
}

static uint8_t xor_reduction16(uint16_t x) {
	x ^= x >> 8;
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return (uint8_t)x & 1;
}

static uint8_t xor_reduction32(uint32_t x) {
	x ^= x >> 16;
	x ^= x >> 8;
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return (uint8_t)x & 1;
}

static uint8_t xor_reduction64(uint64_t x) {
	x ^= x >> 32;
	x ^= x >> 16;
	x ^= x >> 8;
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return (uint8_t)x & 1;
}

int main(int argc, char **argv) {
    srand(121324314);

    int col_length = atoi(argv[1]);
    int num_vals = 1024 << (atoi(argv[2]));

	int col_length_bytes = col_length / 8;
	int total_bytes = col_length_bytes * num_vals;

    // allocate operands data
	void *vals = random_array(total_bytes);

    // allocate output
    uint8_t *output = allocate_array(num_vals);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);
		uint8_t *v8 = (uint8_t *)vals;
		uint16_t *v16 = (uint16_t *)vals;
		uint32_t *v32 = (uint32_t *)vals;
		uint64_t *v64 = (uint64_t *)vals;
			
		switch(col_length) {
			case 8:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = xor_reduction8(v8[i]);
				}
				break;
			case 16:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = xor_reduction16(v16[i]);
				}
				break;
			case 32:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = xor_reduction32(v32[i]);
				}
				break;
			case 64:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = xor_reduction64(v64[i]);
				}
				break;
		}
   	}
    m5_dump_stats(0,0);

	// dummy output
	uint64_t s = 0;
	for(int i = 0; i < num_vals / 8; ++i) {
		s += ((uint64_t *)output)[i];
	}
	printf("%lu\n", s);

    return 0;
}
