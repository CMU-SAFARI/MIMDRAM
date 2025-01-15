#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "simdram.h"

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
void kernel(int *a, int *b, int *c, int *d, int size)
{
    clock_t begin = clock();
    for (int i = 0; i < size; i++)
        d[i] = a[i] + b[i] - c[i];
    clock_t end = clock();

    double time = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent: %fs\n", time);
}

int main(void)
{
    int *a = malloc(sizeof(int[MEM_SIZE]));
    int *b = malloc(sizeof(int[MEM_SIZE]));
    int *c = malloc(sizeof(int[MEM_SIZE]));
    int *d = malloc(sizeof(int[MEM_SIZE]));

    fill(a, MEM_SIZE);
    fill(b, MEM_SIZE);
    fill(c, MEM_SIZE);
    fill(d, MEM_SIZE);
    flush_cache();

    kernel(a, b, c, d, MEM_SIZE);

    free(a);
    free(b);
    free(c);
    free(d);

    return zero;
}
