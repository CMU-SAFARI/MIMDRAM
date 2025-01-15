#include <stdlib.h>

#define VECTOR (sizeof(int[65536]))
#define NOINLINE __attribute__((noinline))

NOINLINE int *fooC(void) {
    int *c = malloc(VECTOR * 2);
    c += VECTOR;
    return c;
}

NOINLINE void fooD(int **d) {
    *d = malloc(VECTOR);
}

NOINLINE int *fooE(int offset) {
    int *e = malloc(VECTOR * 2);
    return e + offset + 32768;
}

NOINLINE int* foo(int *a) {
    int *b = malloc(VECTOR);
    int *c = fooC();
    int *d; fooD(&d);
    int *e = fooE(32768);
    int *f[16]; f[0] = malloc(VECTOR);
    struct G {int *g;} g; g.g = malloc(VECTOR);

    /* Which of those malloc calls can we detect at this point?
           Detected: a, b, c
           Not Detected: d, e, f[0], g.g                     */
}

int main(void) {
    int *a = malloc(VECTOR);
    foo(a);
    return 0;
}
