#pragma once

#include "base.h"

void init_ldcapture();
bool is_initialized_ldcapture();
void shutdown_ldcapture();
bool is_shutdown_done_ldcapture();

// settings.c
void init_settings();
void shutdown_settings();

// encoder.c
void init_encoder();
void shutdown_encoder();

// video_opengl_x11.c
void init_video_opengl_x11();

// audio_fmod.c
void init_audio_fmod();
void shutdown_audio_fmod();

#ifdef DEPRECATED_AUDIO_PULSEAUDIO
    // audio_pulseaudio.c
    void init_audio_pulseaudio();
    void shutdown_audio_pulseaudio();
    bool is_shutdown_done_audio_pulseaudio();
#endif // DEPRECATED_AUDIO_PULSEAUDIO

// timing_linux.c
void init_timing_linux();

// signal_linux.c
void init_signal_linux();