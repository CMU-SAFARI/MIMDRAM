#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "baseline.h"

#define ITERATE(type_t) ({ \
		type_t *v1 = (type_t *)vals1; \
		type_t *out = (type_t *)output; \
		for (int i = 0; i < num_vals; ++i) { \
			type_t t = v1[i]; \
			out[i] = (t>=0)? t:-t; \
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
    void *output = allocate_array(total_bytes);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);
			
		switch(col_length) {
			case 8:
				ITERATE(int8_t);
				break;
			case 16:
				ITERATE(int16_t);
				break;
			case 32:
				ITERATE(int32_t);
				break;
			case 64:
				ITERATE(int64_t);
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
