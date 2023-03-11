#include "math.h"

unsigned int math_mul_div(unsigned int a, unsigned int b, unsigned int c)
{
    #ifdef _M_IX86
        __asm
        {
            mov   eax, [a];
            mul   [b];
            div   [c];
            mov   [a], eax;
        }
        return a;
    #else
        return (unsigned int)(((unsigned long)a * (unsigned long)b) / (unsigned long)c);
#endif
}