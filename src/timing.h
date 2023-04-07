#pragma once

#include "base.h"

void timing_start();
void timing_stop();

bool timing_is_first_frame();
bool timing_is_running();

void timing_next_frame();

void timing_mark_video_ready();
void timing_mark_sound_done();
bool timing_is_video_ready();
bool timing_is_sound_done();

bool timing_is_realtime_frame_done();

i32 timing_get_current_frame();
