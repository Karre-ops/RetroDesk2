/* setjmp.h - C-Laufzeit fuer RD2/32 (RD32C.LIB) */
#ifndef RD32C_SETJMP_H
#define RD32C_SETJMP_H
typedef unsigned long jmp_buf[6];
int  setjmp(jmp_buf b);
void longjmp(jmp_buf b, int v);
#endif
