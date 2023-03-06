#include "init.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static bool initialized = false;

static void bye()
{
    printf("Shutting down from atexit!\n");
    shutdown_ldcapture();
}

void init_ldcapture()
{
    if (initialized) return;

    init_video_opengl_x11();

    init_sound_pulseaudio();

    init_timing_linux();
    
    init_signal_linux();

    atexit(bye);

    initialized = true;
}

void shutdown_ldcapture()
{
    if (!initialized) return;

    shutdown_sound_pulseaudio();

    initialized = false;
}

bool is_shutdown_done_ldcapture()
{
    return is_shutdown_done_sound_pulseaudio();
}