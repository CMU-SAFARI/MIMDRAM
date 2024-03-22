#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>

#define ROW_SIZE 8192
#define BANK_COUNT 16
#define RANK_COUNT 2
#define ALIGNMENT (ROW_SIZE * BANK_COUNT * RANK_COUNT)

extern void rowop_and(void *d, void *s1, void *s2);
extern void rowop_or(void *d, void *s1, void *s2);
extern void rowop_not(void *d, void *s);



void set_one(__m128i *ptr) {
    unsigned *val = (unsigned *) ptr;
    val[0] = val[1] = val[2] = val[3] = 0xFFFFFFFF;
    
}

void set_zero(__m128i *ptr) {
    unsigned *val = (unsigned *) ptr;
    val[0] = val[1] = val[2] = val[3] = 0x0;
    
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

#define FOR_ALL_ROWS for(int i = 0; i < per_col_rows; i ++)
#define ROW(ptr) ((void *)(ptr) + i*ROW_SIZE)

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
    int per_col_rows = (per_col_bytes + ROW_SIZE - 1) / ROW_SIZE;

    unsigned c1, c2;
    int dummy;

    // generate c1 and c2;
    c1 = rand();
    c2 = rand();

    // allocate col data
    unsigned **vals;
    vals = malloc(col_length * sizeof(unsigned *));
    for (int i = 0; i < col_length; i ++)
        dummy = posix_memalign((void *) &(vals[i]), ALIGNMENT, per_col_bytes);

    // allocate output
    unsigned *output;
    dummy += posix_memalign((void *) &output, ALIGNMENT, per_col_bytes);

    unsigned result = 0;
    
    // allocate intermediate vectors
    unsigned *meq1, *meq2, *mlt, *mgt, *neg, *tmp;
    dummy += posix_memalign((void *) &meq1, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &meq2, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &mlt, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &mgt, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &neg, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &tmp, ALIGNMENT, per_col_bytes);
    
    // initialize col data
    for (int j = 0; j < col_length; j ++) {
        for (int i = 0; i < per_col_ints; i ++) {
            vals[j][i]  = rand();
        }
    }

    // set meq1 and meq2 to 1
    for (int i = 0; i < per_col_ints; i ++) {
        meq1[i] = 1;
        meq2[i] = 1;
        mlt[i] = 0;
        mgt[i] = 0;
    }
    
    // run the query
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

        
        for (int j = 0; j < col_length; j ++) {
            unsigned *vec = vals[j];
            FOR_ALL_ROWS { rowop_not(ROW(neg), ROW(vec)); }
            int test1 = !!(c1 & (1 << j));
            int test2 = !!(c2 & (1 << j));

            if (test1 == 0 && test2 == 0) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(tmp), ROW(meq1), ROW(vec));
                    rowop_or(ROW(mgt), ROW(mgt), ROW(tmp));
                    rowop_and(ROW(meq1), ROW(meq1), ROW(neg));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(neg));
                }
            }
            else if (test1 == 1 && test2 == 0) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(meq1), ROW(meq1), ROW(vec));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(neg));
                }
            }
            else if (test1 == 0 && test2 == 1) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(tmp), ROW(meq1), ROW(vec));
                    rowop_or(ROW(mgt), ROW(mgt), ROW(tmp));
                    rowop_and(ROW(tmp), ROW(meq2), ROW(neg));
                    rowop_or(ROW(mlt), ROW(mlt), ROW(tmp));
                    rowop_and(ROW(meq1), ROW(meq1), ROW(neg));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(vec));
                }
            }
            else if (test1 == 1 && test2 == 1) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(tmp), ROW(meq2), ROW(neg));
                    rowop_or(ROW(mlt), ROW(mlt), ROW(tmp));
                    rowop_and(ROW(meq1), ROW(meq1), ROW(vec));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(vec));
                }
            }
        }

        result = 0;
        FOR_ALL_ROWS {
            rowop_and(ROW(output), ROW(mlt), ROW(mgt));
            unsigned *vals = (unsigned *)(ROW(output));
            for (int j = 0; j < ROW_SIZE / 4; j ++)
                result += upopcount(vals[j]);
        }
    }
    m5_dump_stats(0,0);

    printf("%u\n", result);
    
    return 0;
}
