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


    // allocate operands data
    unsigned **vals = random_vector_array(per_col_bytes, col_length);

    // allocate output
    unsigned **output = allocate_vector_array(per_col_bytes, col_length);

	// allocate temporary data
	unsigned **carries;
	if(per_col_bytes <= ALIGNMENT) {
		carries = allocate_vector_array(per_col_bytes, col_length);
	} else {
		carries = allocate_vector_array(ALIGNMENT, col_length);
	}

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		FOR_ALL_VECTORS {
			unsigned *sign = VECTOR(vals[col_length-1]);		// Is 1 iff value is negative

			AAP_VECTORS (B_T0, sign)
			AAP_VECTORS (carries[0], B_T0)
			
			for(int j = 0; j < col_length - 1; j++) {
				unsigned *v = VECTOR(vals[j]);
				unsigned *c_out = carries[j + 1];

				AAP_VECTORS (B_T3, C_0)
				AAP_VECTORS (B_DCC1N, v)
				AAP_VECTORS (c_out, B_DCC1_T0_T3)
			}

			for(int j = 0; j < col_length; j++) {
				unsigned *v    = VECTOR(   vals[j]);
				unsigned *c_in = carries[j];
				unsigned *out  = VECTOR(output[j]);
				
				AAP_VECTORS (B_DCC0N_T0, sign)
				AAP_VECTORS (B_DCC1N_T1, c_in)
				AAP_VECTORS (B_T3, v)
				AAP_VECTORS (B_T3, B_DCC0_T1_T2)
				AAP_VECTORS (B_T2, B_DCC1_T0_T3)
				AAP_VECTORS (B_DCC0N, v)
				AAP_VECTORS (out, B_DCC0_T1_T2)
			}
		}

    }
    m5_dump_stats(0,0);

    return 0;
}
