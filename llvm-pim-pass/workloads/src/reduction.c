#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "simdram.h"

// 4GB of data in each array
#define MEM_SIZE SIMDRAM_VECTOR_SIZE * 10000

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
int kernel(int *a, int *b, unsigned long size)
{
    clock_t begin = clock();
    int tmp = 0;
    for (int i = 0; i < size; i++)
        tmp = tmp + a[i] * b[i];
    clock_t end = clock();

    double time = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent: %fs\n", time);
    return tmp;
}

int main(void)
{
    int *a = malloc(sizeof(int[MEM_SIZE]));
    int *b = malloc(sizeof(int[MEM_SIZE]));

    fill(a, MEM_SIZE);
    fill(b, MEM_SIZE);
    flush_cache();

	kernel(a, b, MEM_SIZE);

	free(a);
	free(b);

	return 0;
}
