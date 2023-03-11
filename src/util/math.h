#pragma once

#include "base.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

u32 math_mul_div(u32 a, u32 b, u32 c);