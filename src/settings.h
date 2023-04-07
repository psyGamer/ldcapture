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
const char* settings_outfile_type();
codec_option_t* settings_codec_options();
i32 settings_codec_options_length();