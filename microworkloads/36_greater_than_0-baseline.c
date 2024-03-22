#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "baseline.h"

#define ITERATE(type_t) ({ \
		type_t *v1 = (type_t *)vals1; \
		for (int i = 0; i < num_vals; ++i) { \
			output[i] = v1[i] = 0; \
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

    // allocate output
    uint8_t *output = allocate_array(num_vals);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);
			
		switch(col_length) {
			case 8:
				ITERATE(uint8_t);
				break;
			case 16:
				ITERATE(uint16_t);
				break;
			case 32:
				ITERATE(uint32_t);
				break;
			case 64:
				ITERATE(uint64_t);
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
