#pragma once

#include "base.h"
#include "encoder.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

typedef struct output_stream_t
{
    AVStream* stream;
    AVCodecContext* codec_ctx;

    AVFrame* in_frame; // For writing data into it
    AVFrame* out_frame; // For writing to a file

    AVPacket* packet;

    struct SwrContext* swr_ctx;

    i32 samples_count;
    size_t frame_sample_pos;
} output_stream_t;

typedef struct encoder_ffmpeg_t
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

    u32 sound_data_channels;
    u32 sound_data_samples;
    encoder_sound_format_t sound_data_format;
    size_t sound_data_size;
    size_t sound_data_buffer_size;

    AVFormatContext* format_ctx;
    bool has_video;
    bool has_audio;
    output_stream_t video_stream;
    output_stream_t audio_stream;
} encoder_ffmpeg_t;

void encoder_ffmpeg_create(encoder_ffmpeg_t* encoder);
void encoder_ffmpeg_destroy(encoder_ffmpeg_t* encoder);

void encoder_ffmpeg_prepare_video(encoder_ffmpeg_t* encoder, u32 width, u32 height);
void encoder_ffmpeg_prepare_sound(encoder_ffmpeg_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format);
void encoder_ffmpeg_flush_video(encoder_ffmpeg_t* encoder);
void encoder_ffmpeg_flush_sound(encoder_ffmpeg_t* encoder);