#include "encoder_qoi.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define QOI_IMPLEMENTATION
#include <qoi.h>

void encoder_qoi_create(encoder_qoi_t* encoder)
{
    // Create "frames" directory if doesn't exist
    struct stat st = { 0 };
    if (stat("./frames", &st) == -1) mkdir("./frames", 0700);
}

void encoder_qoi_destory(encoder_qoi_t* encoder)
{
    fclose(encoder->out_file);
}

void encoder_qoi_save_frame(encoder_qoi_t* encoder)
{
    size_t stride = encoder->channels*  encoder->width;
    stride += (stride % 4) ? (4 - stride % 4) : 0;

    i32 length;
    qoi_desc desc = {
        .width = encoder->width, .height = encoder->height,
        .channels = encoder->channels, .colorspace = QOI_SRGB,
    };

    char fileName[1024]; // Should be long enough
    sprintf(fileName, "./frames/%05d.qoi", encoder->frame_count);

    qoi_write(fileName, encoder->data, &desc);
}