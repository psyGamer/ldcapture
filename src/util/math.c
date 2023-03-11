#include "math.h"

u32 math_mul_div(u32 a, u32 b, u32 c)
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
        return (u32)(((u64)a * (u64)b) / (u64)c);
#endif
}