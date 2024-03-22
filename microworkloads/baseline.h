#include <stdio.h>
#include <stdint.h>

void *allocate_array(size_t size) {
	return malloc(size);
}

void *random_array(size_t size) {
	void *arr = allocate_array(size);
	for(int i = 0; i < size/4; i++) {
		((uint32_t *)arr)[i] = rand();
	}
	return arr;
}
