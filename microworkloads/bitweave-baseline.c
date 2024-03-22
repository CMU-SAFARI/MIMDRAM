#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
//#include <emmintrin.h>
#include <immintrin.h>
#include <m5op.h>

void set_one(__m128i *ptr) {
    unsigned *val = (unsigned *) ptr;
    val[0] = val[1] = val[2] = val[3] = 0xFFFFFFFF;
    
}

void set_zero(__m128i *ptr) {
    unsigned *val = (unsigned *) ptr;
    val[0] = val[1] = val[2] = val[3] = 0x0;
}

void set_rand(__m128i *ptr) {
    unsigned *val = (unsigned *) ptr;
    val[0] = rand();
    val[1] = rand();
    val[2] = rand();
    val[3] = rand();
}

unsigned upopcount(unsigned val) {
    val = val - ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    return ((((val + (val >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24);
}

unsigned popcount(__m128i *ptr) {
    unsigned *val = (unsigned *) ptr;
    return upopcount(val[0]) +
        upopcount(val[1]) +
        upopcount(val[2]) +
        upopcount(val[3]);
}

int main(int argc, char **argv) {
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

    unsigned c1, c2;
    int dummy;

    // generate c1 and c2;
    c1 = rand();
    c2 = rand();

    // allocate col data
    unsigned *vals;
    dummy = posix_memalign((void *) &vals, 16, total_bytes);

    // allocate output
    unsigned *output;
    dummy += posix_memalign((void *) &output, 16, per_col_bytes);
    
    // create simd pointer
    __m128i *vecvals = (__m128i *) (vals);
    __m128i *outputvecs = (__m128i *) (vecvals);
    
    // initialize col data
    // number of segments = per_col_vecs
    for (int i = 0; i < col_length; i ++) {
        for (int j = 0; j < per_col_vecs; j += col_length) {
            set_rand(vecvals + i + j);
        }
    }

    // allocate c1/c2 vec data
    unsigned *c1_vals;
    unsigned *c2_vals;
    dummy += posix_memalign((void *) &c1_vals, 16, col_length * 16);
    dummy += posix_memalign((void *) &c2_vals, 16, col_length * 16);

    __m128i *c1vecs = (__m128i *) (c1_vals);
    __m128i *c2vecs = (__m128i *) (c2_vals);

    unsigned result;
    
    // run the query
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);
        result = 0;
        // set c1 vec
        for (int i = 0; i < col_length; i ++) {
            if (c1 & (1 << i))
                set_one(c1vecs + i);
            else
                set_zero(c1vecs + i);
        }

        // set c2 vec
        for (int i = 0; i < col_length; i ++) {
            if (c2 & (1 << i))
                set_one(c2vecs + i);
            else
                set_zero(c2vecs + i);
        }

        for (int i = 0; i < per_col_vecs; i ++) {
            // execute query
            __m128i mgt, mlt;
            __m128i meq1, meq2;
            
            set_zero(&mgt);
            set_zero(&mlt);
            set_one(&meq1);
            set_one(&meq2);

            for (int j = 0; j < col_length; j ++) {
                __m128i vec = vecvals[i*col_length + j];
                mgt = mgt | (meq1 & (~c1vecs[j]) & vec);
                mlt = mlt | (meq2 & c2vecs[j] & (~vec));
                meq1 = meq1 & (~(vec ^ c1vecs[j]));
                meq2 = meq2 & (~(vec ^ c2vecs[j]));
                outputvecs[i] = mgt & mlt;
                result += popcount(outputvecs + i);
            }
        }
    }
    m5_dump_stats(0,0);

    printf("%u\n", result);
    
    return 0;
}
