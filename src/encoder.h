#pragma once
#include "base.h"

typedef enum encoder_type_t
{
    ENCODER_TYPE_QOI_PCM,
    ENCODER_TYPE_FFMPEG,
} encoder_type_t;

typedef enum encoder_audio_format_t
{
    ENCODER_AUDIO_FORMAT_PCM_S16,
    ENCODER_AUDIO_FORMAT_PCM_F32,
} encoder_audio_format_t;

typedef struct encoder_t
{
    encoder_type_t type;

    u32 video_channels;
    u32 video_width;
    u32 video_height;
    u8* video_data;
    u32 video_frame_count;
    u32 video_row_stride;

    u32 audio_channels;
    u32 audio_freqency;
    encoder_audio_format_t audio_format;
    u8* audio_data;
    size_t audio_byte_count;

    const char save_directory[256];
} encoder_t;

encoder_t* encoder_get_current();

void encoder_create(encoder_t* encoder, encoder_type_t type);
void encoder_destroy(encoder_t* encoder);

void encoder_prepare_video(encoder_t* encoder, u32 width, u32 height);
void encoder_prepare_audio(encoder_t* encoder, u32 channelCount, size_t sampleCount, encoder_audio_format_t format);
void encoder_flush_video(encoder_t* encoder);
void encoder_flush_audio(encoder_t* encoder);

size_t get_audio_format_size(encoder_audio_format_t format);