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

    // generate col data
    unsigned **vals1 = random_vector_array(per_col_bytes, col_length);
    unsigned **vals2 = random_vector_array(per_col_bytes, col_length);

    // allocate output
    unsigned **output = allocate_vector_array(per_col_bytes, 2*col_length);

	// allocate temporary variable
	int vector_width = ALIGNMENT;
	if (per_col_bytes < vector_width) {
		vector_width = per_col_bytes;
	}
	unsigned **rest = allocate_vector_array(vector_width, col_length);
	unsigned **tmp = allocate_vector_array(vector_width, col_length);
	unsigned *m  = allocate_vector(vector_width);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		FOR_ALL_VECTORS {
			for(int j = col_length - 1; j >= 0; j--) {
				// Step 1
				AAP_VECTORS (B_T0, VECTOR(vals1[j]))	
				AAP_VECTORS (rest[j], B_T0            )
				
				// Step 2
				AAP_VECTORS (B_T1, C_1)
				for(int k = 0; k < col_length - j; k ++) {
					unsigned *r  = rest[j + k];
					unsigned *v2 = VECTOR(vals2[k]);

					AAP_VECTORS (B_T2        , r )
					AAP_VECTORS (B_DCC0N     , v2)
					AP_VECTOR   (B_DCC0_T1_T2    )
				}
				for(int k = col_length - j; k < col_length - 1; k += 2) {
					AAP_VECTORS (B_T2_T3, C_0)

					unsigned *v2 = VECTOR(vals2[k]);
					AAP_VECTORS (B_DCC0N, v2)
					AAP_VECTORS (B_T0   , B_DCC0_T1_T2)
		
					v2 = VECTOR(vals2[k+1]);
					AAP_VECTORS (B_DCC1N, v2)
					AAP_VECTORS (B_T1   , B_DCC1_T0_T3)
				}
				if(1 == j % 2) {
					AAP_VECTORS (B_T2   , C_0                        )
					AAP_VECTORS (B_DCC0N, VECTOR(vals2[col_length-1]))
					AAP_VECTORS (m      , B_DCC0_T1_T2               )
				} else {
					AAP_VECTORS (m, B_T1)
				}

				// Step 3
				AAP_VECTORS (VECTOR(output[j]), m)
		
				// Step 4
				for (int k = 0; k < col_length - j - 1; k += 2) {
					AAP_VECTORS(B_T0_T1_T2, C_0)
					AAP_VECTORS(B_T2_T3   , m  )

					unsigned *v2 = VECTOR(vals2[k]);
					unsigned *out = tmp[k];
					AAP_VECTORS (B_DCC0, v2)
					AAP_VECTORS (out, B_DCC0_T1_T2)

					v2 = VECTOR(vals2[k+1]);
					out = tmp[k+1];
					AAP_VECTORS (B_DCC1, v2)
					AAP_VECTORS (out, B_DCC1_T0_T3)
				}
				if (1 == (col_length - j) % 2) {
					unsigned *v2 = VECTOR(vals2[col_length - j - 1]);
					unsigned *out = tmp[col_length - j - 1];

					AAP_VECTORS (B_T0, C_0       )
					AAP_VECTORS (B_T1, m         )
					AAP_VECTORS (B_T2, v2        )
					AAP_VECTORS (out , B_T0_T1_T2)
				}
				
				// Step 5
				AAP_VECTORS (B_DCC0, C_1)
				for (int k = 0; k < col_length - j; k ++) {
					unsigned *r = rest[j + k];
					unsigned *t = tmp[k];
					unsigned *out = r;

					AAP_VECTORS (B_T0_T1_T2, B_DCC0      )
					AAP_VECTORS (B_T2_T3   , r           )
					AAP_VECTORS (B_DCC1N   , t           )
					AAP_VECTORS (B_DCC0    , B_DCC1_T0_T3)
					AAP_VECTORS (B_T0_T3   , B_DCC1N     )
					AP_VECTOR   (B_T0_T1_T2              )
					AAP_VECTORS (B_DCC1N   , t           )
					AAP_VECTORS (out       , B_DCC1_T0_T3)
				}
			}
    	}
	}
    m5_dump_stats(0,0);

    return 0;
}
