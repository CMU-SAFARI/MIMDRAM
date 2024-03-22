#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

unsigned upopcount(unsigned val) {
    val = val - ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    return ((((val + (val >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24);
}

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

    unsigned c1, c2;

    // generate c1 and c2;
    c1 = rand();
    c2 = rand();

    // allocate col data
    unsigned **vals = random_vector_array(per_col_bytes, col_length);

    // allocate output
    unsigned *output = allocate_vector(per_col_bytes);

    unsigned result = 0;
    
    // run the query
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		FOR_ALL_VECTORS {

			AAP_VECTORS( B_DCC1,     C_1 )
			AAP_VECTORS( B_T0_T1_T2, C_1 )

			for (int j = 0; j < col_length; j ++) {
				
				AAP_VECTORS( B_DCC0N_T0, VECTOR(vals[j]));
				
				int test1 = !!(c1 & (1 << j));
				int test2 = !!(c2 & (1 << j));
				
				if (test1 == 0 && test2 == 0) {
					AAP_VECTORS( B_DCC1N_T1, C_0)
				}
				else if (test1 == 0 && test2 == 1) {
					AAP_VECTORS( B_T2_T3, C_1)
				}
				else if (test1 == 1 && test2 == 0) {
					AAP_VECTORS( B_T2_T3, C_0)
				}
				else if (test1 == 1 && test2 == 1) {
					AAP_VECTORS( B_DCC1N_T1, C_1)
				}
				AP_VECTOR( B_DCC1_T0_T3 )
				AP_VECTOR( B_DCC0_T1_T2 )
			}

			AAP_VECTORS( B_T1, C_0 )
			AAP_VECTORS( VECTOR(output), B_T1_T2_T3 )

		}

        result = 0;
		for (int j = 0; j < per_col_ints; j ++) {
			result += upopcount(output[j]);
		}
    }
    m5_dump_stats(0,0);

    printf("%u\n", result);
    
    return 0;
}
