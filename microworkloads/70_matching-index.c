#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint32_t numberOfSetBits(uint32_t i) {
     // Java: use >>> instead of >>
     // C or C++: use uint32_t
     i = i - ((i >> 1) & 0x55555555);
     i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
     return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

int main(int argc, char **argv) {
    srand(0);

    volatile uint32_t dim = 128;
    volatile uint32_t *matrix = (uint32_t *) calloc(dim * dim / 8, 1);
    volatile uint32_t *common_neighbors = (uint32_t *) calloc(dim*dim, sizeof(uint32_t));
    volatile uint32_t *total_neighbors = (uint32_t *) calloc(dim*dim, sizeof(uint32_t));
    volatile float *result = (float *) calloc(dim*dim, sizeof(float));

    for (int i = 0; i < dim * dim / sizeof(uint32_t) / 8; i++) {
        matrix[i] = rand();
    }

    m5_reset_stats(0,0);

    uint32_t tmp;
    for (uint32_t i = 0; i < dim; i++) {
        for (uint32_t j = i+1; j < dim; j++) {
            for (uint32_t k = 0; k < dim / 8 / sizeof(uint32_t); k++) {
                tmp = matrix[(i*dim + k) / sizeof(uint32_t) / 8] & matrix[(j*dim + k) / sizeof(uint32_t) / 8];
                common_neighbors[i*dim + j] += numberOfSetBits(tmp);

                tmp = matrix[(i*dim + k) / sizeof(uint32_t) / 8] | matrix[(j*dim + k) / sizeof(uint32_t) / 8];
                total_neighbors[i*dim + j] += numberOfSetBits(tmp);

                // printf("%u\n", matrix[i*dim + k] & matrix[j*dim + k]);
                // printf("%u\n", matrix[i*dim + k] | matrix[j*dim + k]);
                // printf("%u\n\n", common_neighbors[i*dim + j]);
                // printf("%u\n\n", total_neighbors[i*dim + j]);
            }
        }
    }

    // getchar();
    
    for (uint32_t i = 0; i < dim; i++) {
        for (uint32_t j = i+1; j < dim; j++) {
            if (!common_neighbors[i*dim + i+1+j] && !total_neighbors[i*dim + i+1+j])
                result[i*dim + j] = -1;
            else
                result[i*dim + j] = (float)(common_neighbors[i*dim + i+1+j]) / total_neighbors[i*dim + i+1+j];
        }
    }

    m5_dump_stats(0,0);

    // for (uint32_t i = 0; i < dim; i++) {
    //     for (uint32_t j = i+1; j < dim; j++) {
    //         printf("%f\n", result[i*dim + j]);
    //     }
    // }

    return 0;
}
