#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

int main(int argc, char **argv) {
    init_ambit();
    srand(121324314);

    int col_length = atoi(argv[1]);
    int num_vals = 1024 << (atoi(argv[2]));

    int total_bits = col_length * num_vals;
    int total_bytes = total_bits / 8;
    int total_ints = total_bits / 32;
    int total_vecs = total_bits / 128;

    int per_col_bits = num_vals;
    int per_col_bytes = num_vals / 8;
    int per_col_ints = num_vals / 32;
    int per_col_vecs = num_vals / 128;
    int per_col_rows = (per_col_bytes + ROW_SIZE - 1) / ROW_SIZE;

    // prepare operands data
    unsigned **vals = random_vector_array(per_col_bytes, col_length);

    // allocate output
    unsigned *output = allocate_vector(per_col_bytes);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		FOR_ALL_VECTORS {
			int j;
			// Compute the or reduction of all bits except the msb
			if(col_length % 2 == 1) {
				AAP_VECTORS (B_T3, C_0)
				j = 0;
			} else {
				AAP_VECTORS (B_T3, VECTOR(vals[0]))
				j = 1;
			}

			for (; j < col_length - 2; j += 2) {
				unsigned *v1 = VECTOR(vals[j]);
				unsigned *v2 = VECTOR(vals[j+1]);

				AAP_VECTORS (B_T0_T1_T2, C_1          )
				AAP_VECTORS (B_DCC1    , v1           )
				AAP_VECTORS (B_T2      , B_DCC1_T0_T3 )
				AAP_VECTORS (B_DCC0    , v2           )
				AAP_VECTORS (B_T3      , B_DCC0_T1_T2 )
			} 

			// Compute an AND between the reduced bit and the negated msb
			AAP_VECTORS (B_DCC1N       , VECTOR(vals[j]))
			AAP_VECTORS (B_T0          , C_0            )
			AAP_VECTORS (VECTOR(output), B_DCC1_T0_T3   )
		}
    }
    m5_dump_stats(0,0);

    return 0;
}
