
#define TWO_BYTE_OP(name, number)         \
        .globl name;                      \
        .func name;                       \
name:                                     \
        .byte 0x0F, 0x04;                 \
        .word number;                     \
        ret;                              \
        .endfunc;

TWO_BYTE_OP(rowop_and, 0x5c)
TWO_BYTE_OP(rowop_or,  0x5d)
TWO_BYTE_OP(rowop_not, 0x5e)
TWO_BYTE_OP(rowop_ap,  0x60) 
TWO_BYTE_OP(rowop_aap, 0x61)
