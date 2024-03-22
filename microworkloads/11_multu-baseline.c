#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "baseline.h"

#define ITERATE(intype_t, outtype_t) ({ \
		intype_t *v1 = (intype_t *)vals1; \
		intype_t *v2 = (intype_t *)vals2; \
		outtype_t *out = (outtype_t *)output; \
		for (int i = 0; i < num_vals; ++i) { \
			out[i] = (outtype_t) v1[i] * v2[i]; \
		} \
	})
 
int main(int argc, char **argv) {
    srand(121324314);

    int col_length = atoi(argv[1]);
    int num_vals = 1024 << (atoi(argv[2]));

	int col_length_bytes = col_length / 8;
	int total_bytes = col_length_bytes * num_vals;

    // allocate operands data
	void *vals1 = random_array(total_bytes);
	void *vals2 = random_array(total_bytes);

    // allocate output
    void *output = allocate_array(2 * total_bytes);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);
			
		switch(col_length) {
			case 8:
				ITERATE(uint8_t, uint16_t);
				break;
			case 16:
				ITERATE(uint16_t, uint32_t);
				break;
			case 32:
				ITERATE(uint32_t, uint64_t);
				break;
		}
   	}
    m5_dump_stats(0,0);

	// dummy output
	uint64_t s = 0;
	for(int i = 0; i < total_bytes / 8; ++i) {
		s += ((uint64_t *)output)[i];
	}
	printf("%lu\n", s);

    return 0;
}
