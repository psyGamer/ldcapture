#pragma once
#include "base.h"

#define LDCAPTURE_CONFIG_FILE "ldcapture.conf"

typedef struct codec_option_t
{
    const char* name;
    const char* value;
} codec_option_t;

i32 settings_fps();
i32 settings_video_width();
i32 settings_video_height();
i32 settings_video_bitrate();
i32 settings_audio_bitrate();
i32 settings_audio_samplerate();

bool settings_overwrite_video_codec();
bool settings_overwrite_audio_codec();
const char* settings_video_codec();
const char* settings_audio_codec();
const char* settings_container_type();

codec_option_t* settings_video_codec_options();
codec_option_t* settings_audio_codec_options();
i32 settings_video_codec_options_length();
i32 settings_audio_codec_options_length();