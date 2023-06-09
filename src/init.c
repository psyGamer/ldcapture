#include "init.h"

static bool initialized = false;

void init_ldcapture()
{
    if (initialized) return;

    TRACE("Initializing...");

    init_settings();

    init_encoder();

    init_video_opengl_x11();
    init_audio_fmod();

    init_timing_linux();
    init_signal_linux();

    atexit(shutdown_ldcapture);

    TRACE("Initialized");

    initialized = true;
}

void shutdown_ldcapture()
{
    if (!initialized) return;

    TRACE("Shutting down...");

    shutdown_audio_fmod();

    while (!is_shutdown_done_ldcapture())
    ;

    shutdown_settings();

    TRACE("Shut down");

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