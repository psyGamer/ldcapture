#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define QOI_IMPLEMENTATION
#include <qoi.h>

#include "encoder_qoi_pcm.h"
#include "filesystem.h"
#include "util/math.h"

const char* ENCODE_VIDEO_SCRIPT =
    "#!/bin/sh\n"
    "printf \"\033[1;93mEncoding video...\033[0m\n\"\n"
    "ffmpeg -y -framerate 60 -pattern_type glob -i 'frames/*.qoi' -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\" -vcodec libx264 -pix_fmt yuv420p output_video.mp4\n"
    "printf \"\033[1;93mEncoding audio...\033[0m\n\"\n"
    "ffmpeg -y -f f32le -ar 48000 -channels 2 -i audio.raw -f wav output_audio.wav\n"
    "printf \"\033[1;93mCombining video & audio...\033[0m\n\"\n"
    "ffmpeg -y -i output_video.mp4 -i output_audio.wav -c:v copy -c:a aac -pix_fmt yuv420p output.mp4";

void encoder_qoi_pcm_create(encoder_qoi_pcm_t* encoder)
{
    encoder->sound_data_channels = 0;
    encoder->sound_data_samples = 0;
    encoder->sound_data_format = 0;
    encoder->sound_data_size = 0;
    encoder->sound_data_buffer_size = 0;

    char path[1024];
    sprintf(path, "%s/audio.raw", encoder->save_directory);
    encoder->sound_file = fopen(path, "wb");

    sprintf(path, "%s/frames", encoder->save_directory);
    if (!directory_exists(path))
        create_directory(path);
    
    sprintf(path, "%s/encode_video.sh", encoder->save_directory);
    FILE* f = fopen(path, "w");
    fwrite(ENCODE_VIDEO_SCRIPT, sizeof(char), strlen(ENCODE_VIDEO_SCRIPT), f);
    fflush(f);
    fclose(f);
}

void encoder_qoi_pcm_destory(encoder_qoi_pcm_t* encoder)
{
    fflush(encoder->sound_file);
    fclose(encoder->sound_file);
}

void encoder_qoi_pcm_prepare_video(encoder_qoi_pcm_t* encoder, u32 width, u32 height)
{
    bool reallocate = (encoder->video_width * encoder->video_height) < (width * height) ||   // Buffer too small
                      (encoder->video_width * encoder->video_height) > (width * height * 2); // Buffer is more than double as big as it should be
    
    encoder->video_width = width;
    encoder->video_height = height;

    if (reallocate)
    {
        free(encoder->video_data);
        encoder->video_data = realloc(encoder->video_data, width * height * encoder->video_channels);
    }
}

void encoder_qoi_pcm_prepare_sound(encoder_qoi_pcm_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format)
{
    size_t srcVarSize = get_sound_format_size(format);
    size_t dstVarSize = get_sound_format_size(encoder->sound_format);

    encoder->sound_data_size = max(channelCount, encoder->sound_channels) * max(srcVarSize, dstVarSize) * sampleCount;
    encoder->sound_data_channels = channelCount;
    encoder->sound_data_format = format;

    if (encoder->sound_data_buffer_size < encoder->sound_data_size)
    {
        free(encoder->sound_data);
        encoder->sound_data_buffer_size = encoder->sound_data_size * 2; // Avoid having to reallocate soon
        encoder->sound_data = realloc(encoder->sound_data, encoder->sound_data_buffer_size);
    }
}

void encoder_qoi_pcm_flush_video(encoder_qoi_pcm_t* encoder)
{
    size_t stride = encoder->video_channels * encoder->video_width;
    stride += (stride % 4) != 0 ? (4 - stride % 4) : 0; // Align on 4 bytes

    qoi_desc desc = {
        .width = encoder->video_width, .height = encoder->video_height,
        .channels = encoder->video_channels, .colorspace = QOI_SRGB,
    };

    char filePath[1024];
    sprintf(filePath, "%s/frames/%05d.qoi", encoder->save_directory, encoder->video_frame_count);
    
    encoder->video_frame_count++;

    qoi_write(filePath, encoder->video_data, &desc);
}

void encoder_qoi_pcm_flush_sound(encoder_qoi_pcm_t* encoder)
{
    // Transcode the data if nessecery
    // No reallocations need since the buffer should already be large enough
    if (encoder->sound_channels != encoder->sound_data_channels)
    {
        u8* origData = malloc(encoder->sound_data_buffer_size);
        memcpy(origData, encoder->sound_data, encoder->sound_data_size);

        size_t dataVarSize = get_sound_format_size(encoder->sound_data_format);

        if (encoder->sound_data_channels > encoder->sound_channels)
        {
            // Cut the remaining channels off
            for (i32 sample = 0; sample < encoder->sound_data_samples; sample++)
            {
                memcpy(encoder->sound_data + sample * encoder->sound_channels, 
                       origData            + sample * encoder->sound_data_channels,
                       encoder->sound_channels * dataVarSize);
            }
        }
        else
        {
            // Repeat the last channel to fill the remaining ones
            for (i32 sample = 0; sample < encoder->sound_data_samples; sample++)
            {
                memcpy(encoder->sound_data + sample * encoder->sound_channels, 
                       origData            + sample * encoder->sound_data_channels,
                       encoder->sound_data_channels * dataVarSize);
                i32 lastDataChannel = encoder->sound_data_channels - 1;
                for (i32 channel = lastDataChannel; channel < encoder->sound_channels; channel++)
                {
                    encoder->sound_data[sample * encoder->sound_channels      + channel]
                             = origData[sample * encoder->sound_data_channels + lastDataChannel];
                }
            }
        }

        encoder->sound_data_size = encoder->sound_data_samples * encoder->sound_channels * dataVarSize;
        free(origData);
    }

    if (encoder->sound_format != encoder->sound_data_format)
    {
        #define IMPL_FORMAT_TRANSCODE(srcType, dstType, act)                                                \
            for (i32 i = 0, j = 0; i < encoder->sound_data_size; i += sizeof(srcType), j += sizeof(dstType)) \
            {                                                                                                \
                *(dstType *)(encoder->sound_data + j) = *(srcType *)(encoder->sound_data + i) act;        \
            }

        switch (encoder->sound_format)
        {
        case ENCODER_SOUND_FORMAT_PCM_S16:
            switch (encoder->sound_data_format)
            {
            case ENCODER_SOUND_FORMAT_PCM_F32:
                IMPL_FORMAT_TRANSCODE(f32, i16, * 32767)
                break;
            default:
                ERROR("Invalid sound format: %i", encoder->sound_data_format);
            }
            break;
        case ENCODER_SOUND_FORMAT_PCM_F32:
            switch (encoder->sound_data_format)
            {
            case ENCODER_SOUND_FORMAT_PCM_S16:
                IMPL_FORMAT_TRANSCODE(i16, f32, / 32767.0f)
                break;
            default:
                ERROR("Invalid sound format: %i", encoder->sound_data_format);
            }
            break;
        }

        encoder->sound_data_size = encoder->sound_data_samples * encoder->sound_channels * get_sound_format_size(encoder->sound_format);
    }

    encoder->sound_byte_count += encoder->sound_data_size;
    fwrite(encoder->sound_data, 1, encoder->sound_data_size, encoder->sound_file);
}


// void encoder_qoi_pcm_save_frame(encoder_qoi_pcm_t* encoder)
// {
//     size_t stride = encoder->video_channels*  encoder->video_width;
//     stride += (stride % 4) ? (4 - stride % 4) : 0;

//     i32 length;
//     qoi_desc desc = {
//         .width = encoder->video_width, .height = encoder->video_height,
//         .channels = encoder->video_channels, .colorspace = QOI_SRGB,
//     };

//     char fileName[1024]; // Should be long enough
//     sprintf(fileName, "./frames/%05d.qoi", encoder->video_frame_count);

//     qoi_write(fileName, encoder->video_data, &desc);
// }