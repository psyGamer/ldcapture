#pragma once

#include <stdbool.h>

void init_ldcapture();
void shutdown_ldcapture();
bool is_shutdown_done_ldcapture();

void init_video_opengl_x11(); // video_opengl_x11.c

void init_sound_pulseaudio(); // sound_pulseaudio.c
void shutdown_sound_pulseaudio(); // sound_pulseaudio.c
bool is_shutdown_done_sound_pulseaudio(); // sound_pulseaudio.c

void init_timing_linux(); // timing_linux.c

void init_signal_linux(); // signal_linux.c