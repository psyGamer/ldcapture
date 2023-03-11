#pragma once

#include "base.h"

void init_ldcapture();
bool is_initialized_ldcapture();
void shutdown_ldcapture();
bool is_shutdown_done_ldcapture();

// video_opengl_x11.c
void init_video_opengl_x11();

// sound_fmod.c
void init_sound_fmod5();
void shutdown_soundsys_fmod5();

// audiocapture_pulseaudio.c
void init_audiocapture_pulseaudio();
void shutdown_audiocapture_pulseaudio();
bool is_shutdown_done_audiocapture_pulseaudio();

// timing_linux.c
void init_timing_linux();

// signal_linux.c
void init_signal_linux();