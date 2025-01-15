#ifndef SIMDRAM_H
#define SIMDRAM_H

#define SIMDRAM_VECTOR_SIZE 65536

extern void simdram_transpose(void *ptr, unsigned int size, unsigned int element_size);
extern void simdram_mark_transposed(void *ptr);

#endif //SIMDRAM_H
