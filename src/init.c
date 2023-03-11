#include "init.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static bool initialized = false;

void init_ldcapture()
{
    if (initialized) return;

    init_video_opengl_x11();

    init_sound_fmod5();

    init_timing_linux();
    
    init_signal_linux();

    atexit(shutdown_ldcapture);

    initialized = true;
}

void shutdown_ldcapture()
{
    if (!initialized) return;

    shutdown_soundsys_fmod5();

    initialized = false;
}

bool is_initialized_ldcapture()
{
    return initialized;
}

bool is_shutdown_done_ldcapture()
{
    return true;
}