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
	unsigned *sign;
	if(per_col_bytes <= ALIGNMENT) {
		sign = allocate_vector(per_col_bytes);
	} else {
		sign = allocate_vector(ALIGNMENT);
	}

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		FOR_ALL_VECTORS {
			unsigned *msb = VECTOR(vals[col_length-1]);
			AAP_VECTORS (B_DCC0N, msb   )
			AAP_VECTORS (sign   , B_DCC0)

			int j;
			for(j = 0; j < col_length - 2; j += 2) {
				// Compute 2 consecutive bits together to spend 1 less AAP per bit

				AAP_VECTORS (B_T0_T1_T2, sign)
				AAP_VECTORS (B_T2_T3   , C_0 )

				unsigned *v = VECTOR(vals[j]);
				unsigned *out = VECTOR(output[j]);
				AAP_VECTORS (B_DCC0, v           )
				AAP_VECTORS (out   , B_DCC0_T1_T2)

				v = VECTOR(vals[j+1]);
				out = VECTOR(output[j+1]);
				AAP_VECTORS (B_DCC1, v           )
				AAP_VECTORS (out   , B_DCC1_T0_T3)
			}

			if(col_length - 2 == j) {
				// Compute the second msb
				AAP_VECTORS (B_T0             , sign           )
				AAP_VECTORS (B_T1             , C_0            )
				AAP_VECTORS (B_T2             , VECTOR(vals[j]))
				AAP_VECTORS (VECTOR(output[j]), B_T0_T1_T2     )
			}

			AAP_VECTORS (msb, C_0)		// the msb is garaunteed to be 0
		}
	}
    m5_dump_stats(0,0);

    return 0;
}
