#pragma once

#include "base.h"
#include "encoder.h"

typedef struct encoder_qoi_pcm_t
{
    encoder_type_t type;

    u32 video_channels;
    u32 video_width;
    u32 video_height;
    u8* video_data;
    u32 video_frame_count;

    u32 audio_channels;
    u32 audio_freqency;
    encoder_audio_format_t audio_format;
    u8* audio_data;
    size_t audio_byte_count;

    const char save_directory[256];

    u32 audio_data_channels;
    u32 audio_data_samples;
    encoder_audio_format_t audio_data_format;
    size_t audio_data_size;
    size_t audio_data_buffer_size;
    FILE* audio_file;
} encoder_qoi_pcm_t;

void encoder_qoi_pcm_create(encoder_qoi_pcm_t* encoder);
void encoder_qoi_pcm_destroy(encoder_qoi_pcm_t* encoder);

void encoder_qoi_pcm_prepare_video(encoder_qoi_pcm_t* encoder, u32 width, u32 height);
void encoder_qoi_pcm_prepare_audio(encoder_qoi_pcm_t* encoder, u32 channelCount, size_t sampleCount, encoder_audio_format_t format);
void encoder_qoi_pcm_flush_video(encoder_qoi_pcm_t* encoder);
void encoder_qoi_pcm_flush_audio(encoder_qoi_pcm_t* encoder);