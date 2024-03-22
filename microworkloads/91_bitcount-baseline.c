#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "baseline.h"

static uint8_t bitcount8(uint8_t x) {
	x = (x & 0x55) + ((x >> 1) & 0x55);
	x = (x & 0x33) + ((x >> 2) & 0x33);
	x = (x & 0x0F) + ((x >> 4) & 0x0F);
	return x;
}

static uint8_t bitcount16(uint16_t x) {
	x = (x & 0x5555) + ((x >> 1) & 0x5555);
	x = (x & 0x3333) + ((x >> 2) & 0x3333);
	x = (x & 0x0F0F) + ((x >> 4) & 0x0F0F);
	return (uint8_t) x + (uint8_t)(x >> 8);
}

static uint8_t bitcount32(uint32_t x) {
	x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x & 0x0F0F0F0F) + ((x >> 4) & 0x0F0F0F0F);
	x = (x & 0x00FF00FF) + ((x >> 8) & 0x00FF00FF);
	return (uint8_t) x + (uint8_t)(x >> 16);
}

static uint8_t bitcount64(uint64_t x) {
	x = (x & 0x5555555555555555) + ((x >> 1) & 0x5555555555555555);
	x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
	x = (x & 0x0F0F0F0F0F0F0F0F) + ((x >> 4) & 0x0F0F0F0F0F0F0F0F);
	x = (x & 0x00FF00FF00FF00FF) + ((x >> 8) & 0x00FF00FF00FF00FF);
	x = (x & 0x0000FFFF0000FFFF) + ((x >> 16) & 0x0000FFFF0000FFFF);
	return (uint8_t) x + (uint8_t)(x >> 32);
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
					output[i] = bitcount8(v8[i]);
				}
				break;
			case 16:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = bitcount16(v16[i]);
				}
				break;
			case 32:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = bitcount32(v32[i]);
				}
				break;
			case 64:
				for (int i = 0; i < num_vals; ++i) {
					output[i] = bitcount64(v64[i]);
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
