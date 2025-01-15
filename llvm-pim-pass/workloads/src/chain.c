#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "simdram.h"

#define MEM_SIZE SIMDRAM_VECTOR_SIZE

void flush_cache(void)
{
    const int size = 1024 * 1024 * 64;
    int *start = malloc(size * sizeof(int));
    int *ptr = start;
    int *end = ptr + size;
    while (ptr != end)
        *(ptr++) = rand();
    free(start);
}

void fill(int *ptr, unsigned long size)
{
    int *end = ptr + size;
    while (ptr != end)
        *(ptr++) = rand();
}

__attribute__((noinline))
void kernel(int A[MEM_SIZE][MEM_SIZE], int B[MEM_SIZE][MEM_SIZE], int _PB_N, int _PB_1)
{
    clock_t begin = clock();
    for (int i1 = 1; i1 < _PB_1; i1++)
        for (int i2 = 0; i2 < _PB_N; i2++)
            B[i1][i2] = B[i1][i2] - A[i1][i2] * A[i1][i2] / B[i1-1][i2];
    clock_t end = clock();

    double time = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent: %fs\n", time);
}

int main(void)
{
    int (*A)[MEM_SIZE] = malloc(sizeof(int[10000][MEM_SIZE]));
    int (*B)[MEM_SIZE] = malloc(sizeof(int[10000][MEM_SIZE]));

    for (int i = 0; i < 10000; i++)
    {
        fill(A[i], MEM_SIZE);
        fill(B[i], MEM_SIZE);
    }
    flush_cache();

    kernel(A, B, MEM_SIZE, 10000);

    free(A);
    free(B);

    return 0;
}
