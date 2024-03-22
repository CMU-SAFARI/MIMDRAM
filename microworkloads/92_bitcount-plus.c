#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

static int per_col_rows;

// TODO find a good name for this function
// This functions returns 2^floor(log2(n)) - 1
int32_t foo(int32_t n) {
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	
	return n >> 1;
}

int floor_log2(uint32_t n) {
	int result = -1;
	while (n > 0) {
		++result;
		n >>= 1;
	}
	return result;
}

/* Compute how many bits are necessary to store all the partial counts.
 *
 * This function emulates the whole algorithm to find the maximum space
 * occupied by the partial counts.
 */
int necessary_space(int col_length) {
	if (col_length <= 2) {
		return col_length;
	} else if (col_length == 3) {
		return 2;
	}
	int left = foo(col_length);
	int right = col_length - 1 - left;
	
	int flog2 = floor_log2(col_length);
	int lower_limit1 = flog2 + 1;
	int lower_limit2 = flog2 + necessary_space(right);
	int lower_limit3 = necessary_space(left);
	
	int space = lower_limit1;
	if (space < lower_limit2) {
		space = lower_limit2;
	}
	if (space < lower_limit3) {
		space = lower_limit3;
	}
	return space;
}
	
/* Count bits in the columns of the input vectors and store the result
 * in the columns of the output vectors.
 *
 * Return the number of bits used to store the result.
 * The variable output must have at least TODO vectors.
 *
 */
int bitcount(unsigned **input, int input_size, unsigned **output) {
	switch (input_size) {
		case 0:
			return 0;
		case 1:
			FOR_ALL_VECTORS {
				AAP_VECTORS (VECTOR(output[0]), VECTOR(input[0]))
			}
			return 1;
		case 2:	// fallthrough
		case 3:
			FOR_ALL_VECTORS {
				unsigned *sum = VECTOR(output[0]);
				unsigned *c_out = VECTOR(output[1]);
				unsigned *v1 = VECTOR(input[0]);
				unsigned *v2 = VECTOR(input[1]);
				unsigned *v3;
				if (2 == input_size) {
					v3 = C_0;
				} else {
					v3 = VECTOR(input[2]);
				}

				AAP_VECTORS (B_T0_T1_T2, v1          );
				AAP_VECTORS (B_T2_T3   , v2          );
				AAP_VECTORS (B_DCC1    , v3          );
				AAP_VECTORS (c_out     , B_DCC1_T0_T3);
				AAP_VECTORS (B_T0_T3   , B_DCC1N     );
				AP_VECTOR   (B_T0_T1_T2              );
				AAP_VECTORS (B_T1      , v3          );
				AAP_VECTORS (sum       , B_T1_T2_T3  );
			}
			return 2;
	}
	
	// Divide and conquer: divide problem in two parts and one bit to input as carry to adder
	int left = foo(input_size);
	int right = input_size - left - 1;	// -1 because we leave out one bit
	int b1 = bitcount(input       , left , output     );
	int b2 = bitcount(input + left, right, output + b1);

	FOR_ALL_VECTORS {
		unsigned *c_in = VECTOR(input[left]);
		unsigned *c_out = VECTOR(output[b1]);

		for (int i = 0; i < b1; i ++) {
			unsigned *v1 = VECTOR(output[i]);
			unsigned *v2;
			if (i < b2) {
				v2 = VECTOR(output[i+b1]);
			} else {
				v2 = C_0;
			}
			unsigned *out = v1;

			AAP_VECTORS (B_T0_T1_T2  , c_in      );
			AAP_VECTORS (B_T2_T3     , v1        );
			AAP_VECTORS (B_DCC1      , v2        );
			if (i < b1 - 1) {
				AP_VECTOR   (B_DCC1_T0_T3);
			} else {
				AAP_VECTORS (c_out, B_DCC1_T0_T3);
			}
			AAP_VECTORS (B_T0_T3     , B_DCC1N   );
			AP_VECTOR   (B_T0_T1_T2              );
			AAP_VECTORS (B_T1        , v2        );
			AAP_VECTORS (out         , B_T1_T2_T3);

			c_in = B_DCC1;
		}
	}

	return 1 + b1;
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
    per_col_rows = (per_col_bytes + ROW_SIZE - 1) / ROW_SIZE;

    // allocate col data
    unsigned **vals = random_vector_array(per_col_bytes, col_length);

    // allocate output (includes extra bits)
	int output_bits = necessary_space(col_length);
    unsigned **output = allocate_vector_array(per_col_bytes, output_bits);

    // run the query
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		bitcount(vals, col_length, output);

    }

	// trim the output
	for (int i = col_length; i < output_bits; ++i) {
		free(output[i]);
	}
	output = realloc(output, col_length * sizeof(unsigned *));

    m5_dump_stats(0,0);

    return 0;
}
