#pragma once

#include <stdbool.h>

void timing_start();
void timing_stop();

bool timing_is_running();

void timing_next_frame();

void timing_video_done();
void timing_sound_done();
bool timing_is_video_done();
bool timing_is_sound_done();

bool timing_is_realtime_frame_done();

i32 timing_get_target_fps();

i32 timing_get_current_frame();
