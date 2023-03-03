#include "init.h"

#include <stdbool.h>

static bool initialized = false;

void init_ldcapture()
{
    if (initialized) return;

    init_opengl_x11();

    init_timing_linux();

    initialized = true;
}