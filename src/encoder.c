#include <time.h>

#include "encoder.h"
#include "encoder_qoi_pcm.h"
#include "filesystem.h"

static encoder_t* current_encoder = NULL;

void init_encoder()
{
    encoder_type_t type = ENCODER_TYPE_QOI_PCM;

    size_t structSize;
    switch (type)
    {
    case ENCODER_TYPE_QOI_PCM:
        structSize = sizeof(encoder_qoi_pcm_t);
        break;
    }

    current_encoder = malloc(structSize);
    encoder_create(current_encoder, type);
}
void shutdown_encoder()
{
    encoder_destroy(current_encoder);
    free(current_encoder);
}

encoder_t* encoder_get_current() { return current_encoder; }

void encoder_create(encoder_t* encoder, encoder_type_t type)
{
    encoder->type = type;
    encoder->video_channels = 4; // Currently hardcoded
    encoder->video_width = 0;
    encoder->video_height = 0;
    encoder->video_data = NULL;
    encoder->video_frame_count = 0;
    encoder->sound_channels = 2;
    encoder->sound_freqency = 48000;
    encoder->sound_format = ENCODER_SOUND_FORMAT_PCM_F32;
    encoder->sound_data = NULL;
    encoder->sound_byte_count = 0;

    // Create a directory for the recording
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // DD-MM-YYYY_SS-MM-HH
    sprintf((char*)encoder->save_directory, LDCAPTURE_RECORDINGS_DIRECTORY "/%02d-%02d-%d_%02d-%02d-%02d", 
        tm.tm_mday, tm.tm_mon, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

    if (!directory_exists(LDCAPTURE_RECORDINGS_DIRECTORY))
        create_directory(LDCAPTURE_RECORDINGS_DIRECTORY);
    if (!directory_exists(encoder->save_directory))
        create_directory(encoder->save_directory);

    switch (type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_create((encoder_qoi_pcm_t*)encoder);
        break;
    }
}

void encoder_destroy(encoder_t* encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_destory((encoder_qoi_pcm_t*)encoder);
        break;
    }

    free(encoder->video_data);
}

void encoder_prepare_video(encoder_t* encoder, u32 width, u32 height)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_prepare_video((encoder_qoi_pcm_t*)encoder, width, height);
        break;
    }
}

void encoder_prepare_sound(encoder_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_prepare_sound((encoder_qoi_pcm_t*)encoder, channelCount, sampleCount, format);
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
    }
}
void encoder_flush_sound(encoder_t* encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI_PCM:
        encoder_qoi_pcm_flush_sound((encoder_qoi_pcm_t*)encoder);
        break;
    }
}

size_t get_sound_format_size(encoder_sound_format_t format)
{
    switch (format)
    {
    case ENCODER_SOUND_FORMAT_PCM_S16:
        return sizeof(i16);
    case ENCODER_SOUND_FORMAT_PCM_F32:
        return sizeof(f32);
    }

    ERROR("Invalid sound format: %i", format);
    return -1;
}