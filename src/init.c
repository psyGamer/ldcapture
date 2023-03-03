#include "init.h"

#include <stdbool.h>

static bool initialized = false;

void init_ldcapture()
{
    if (initialized) return;

    init_opengl_x11();

    initialized = true;
}