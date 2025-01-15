#include <stdlib.h>
#include "simdram.h"

#define MEM_SIZE SIMDRAM_VECTOR_SIZE * 2


__attribute__((noinline))
void kernel(int A[MEM_SIZE][MEM_SIZE], int *y, int *tmp, int _PB_NX, int _PB_NY)
{
    for (int i = 0; i < _PB_NX; i++)
        for (int j = 0; j < _PB_NY; j++)
            y[j] += A[i][j] * tmp[i];
}

int main(void)
{
    int (*A)[MEM_SIZE] = malloc(sizeof(int[MEM_SIZE][MEM_SIZE]));
    int *y = malloc(sizeof(int[MEM_SIZE]));
    int *tmp = malloc(sizeof(int[MEM_SIZE]));

	kernel(A, y, tmp, MEM_SIZE, MEM_SIZE);

	return 0;
}
