#include <time.h>

#include "encoder.h"
#include "encoder_qoi_pcm.h"
#include "encoder_ffmpeg.h"
#include "filesystem.h"

static encoder_t* current_encoder = NULL;
static encoder_type_t current_encoder_type;

void init_encoder()
{
    encoder_type_t type = ENCODER_TYPE_FFMPEG;

    size_t structSize;
    switch (type)
    {
    case ENCODER_TYPE_QOI_PCM:
        structSize = sizeof(encoder_qoi_pcm_t);
        break;
    case ENCODER_TYPE_FFMPEG:
        structSize = sizeof(encoder_ffmpeg_t);
        break;
    }

    current_encoder = malloc(structSize);
}
void shutdown_encoder()
{
    free(current_encoder);
}

encoder_t* encoder_get_current() { return current_encoder; }

void encoder_create(encoder_t* encoder, encoder_type_t type)
{
    encoder->type = type;
    encoder->video_channels = 4; // Currently hardcoded
    encoder->video_width = 1920;
    encoder->video_height = 1080;
    encoder->video_data = NULL;
    encoder->video_frame_count = 0;
    encoder->video_row_stride = 0;
    encoder->audio_channels = 2;
    encoder->audio_freqency = 48000;
    encoder->audio_format = ENCODER_AUDIO_FORMAT_PCM_F32;
    encoder->audio_data = NULL;
    encoder->audio_byte_count = 0;

    // Create a directory for the recording
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // DD-MM-YYYY_SS-MM-HH
    sprintf((char*)encoder->save_directory, LDCAPTURE_RECORDINGS_DIRECTORY "/%02d-%02d-%d_%02d-%02d-%02d", 
        tm.tm_mday, tm.tm_mon, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

    if (!directory_exists(LDCAPTURE_RECORDINGS_DIRECTORY))
        create_directory(LDCAPTURE_RECORDINGS_DIRECTORY);

    switch (type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_create((encoder_qoi_pcm_t*)encoder);
        break;
    case ENCODER_TYPE_FFMPEG:
        encoder_ffmpeg_create((encoder_ffmpeg_t*)encoder);
        break;
    }
}

void encoder_destroy(encoder_t* encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_destroy((encoder_qoi_pcm_t*)encoder);
        free(encoder->video_data);
        break;
    case ENCODER_TYPE_FFMPEG:
        encoder_ffmpeg_destroy((encoder_ffmpeg_t*)encoder);
        break;
    }
}

void encoder_prepare_video(encoder_t* encoder, u32 width, u32 height)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_prepare_video((encoder_qoi_pcm_t*)encoder, width, height);
        break;
    case ENCODER_TYPE_FFMPEG:
        encoder_ffmpeg_prepare_video((encoder_ffmpeg_t*)encoder, width, height);
        break;
    }
}

void encoder_prepare_audio(encoder_t* encoder, u32 channelCount, size_t sampleCount, encoder_audio_format_t format)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_prepare_audio((encoder_qoi_pcm_t*)encoder, channelCount, sampleCount, format);
        break;
    case ENCODER_TYPE_FFMPEG:
        encoder_ffmpeg_prepare_audio((encoder_ffmpeg_t*)encoder, channelCount, sampleCount, format);
        break;
    }
}

void encoder_flush_video(encoder_t* encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_flush_video((encoder_qoi_pcm_t*)encoder);
        break;
    case ENCODER_TYPE_FFMPEG:
        encoder_ffmpeg_flush_video((encoder_ffmpeg_t*)encoder);
        break;
    }
}
void encoder_flush_audio(encoder_t* encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_flush_audio((encoder_qoi_pcm_t*)encoder);
        break;
    case ENCODER_TYPE_FFMPEG:
        encoder_ffmpeg_flush_audio((encoder_ffmpeg_t*)encoder);
        break;
    }
}

size_t get_audio_format_size(encoder_audio_format_t format)
{
    switch (format)
    {
    case ENCODER_AUDIO_FORMAT_PCM_S16:
        return sizeof(i16);
    case ENCODER_AUDIO_FORMAT_PCM_F32:
        return sizeof(f32);
    }

    ERROR("Invalid audio format: %i", format);
    return -1;
}