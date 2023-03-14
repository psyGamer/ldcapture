#pragma once

#include "base.h"

void init_ldcapture();
bool is_initialized_ldcapture();
void shutdown_ldcapture();
bool is_shutdown_done_ldcapture();

// encoder.c
void init_encoder();
void shutdown_encoder();

// video_opengl_x11.c
void init_video_opengl_x11();

// sound_fmod.c
void init_sound_fmod();
void shutdown_sound_fmod();

#ifdef DEPRECATED_SOUND_PULSEAUDIO
    // sound_pulseaudio.c
    void init_sound_pulseaudio();
    void shutdown_sound_pulseaudio();
    bool is_shutdown_done_sound_pulseaudio();
#endif // DEPRECATED_SOUND_PULSEAUDIO

// timing_linux.c
void init_timing_linux();

// signal_linux.c
void init_signal_linux();