#pragma once
#include "base.h"

typedef enum encoder_type_t
{
    ENCODER_TYPE_QOI_PCM,
} encoder_type_t;

typedef enum encoder_sound_format_t
{
    ENCODER_SOUND_FORMAT_PCM_S16,
    ENCODER_SOUND_FORMAT_PCM_F32,
} encoder_sound_format_t;

typedef struct encoder_t
{
    encoder_type_t type;

    u32 video_channels;
    u32 video_width;
    u32 video_height;
    u8* video_data;
    u32 video_frame_count;

    u32 sound_channels;
    u32 sound_freqency;
    encoder_sound_format_t sound_format;
    u8* sound_data;
    size_t sound_byte_count;

    const char save_directory[256];
} encoder_t;

encoder_t* encoder_get_current();

void encoder_create(encoder_t* encoder, encoder_type_t type);
void encoder_destroy(encoder_t* encoder);

void encoder_prepare_video(encoder_t* encoder, u32 width, u32 height);
void encoder_prepare_sound(encoder_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format);
void encoder_flush_video(encoder_t* encoder);
void encoder_flush_sound(encoder_t* encoder);

size_t get_sound_format_size(encoder_sound_format_t format);