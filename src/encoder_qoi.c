#include "encoder_qoi.h"

#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define QOI_IMPLEMENTATION
#include <qoi.h>

void encoder_qoi_create(encoder_qoi_t *encoder)
{
    // Create "frames" directory if doesn't exist
    struct stat st = {0};
    if (stat("./frames", &st) == -1) mkdir("./frames", 0700);

    encoder->outFile = fopen("./frames/out.raw", "wb");
}

void encoder_qoi_destory(encoder_qoi_t *encoder)
{
    fclose(encoder->outFile);
}

void encoder_qoi_save_frame(encoder_qoi_t *encoder)
{
    printf("Writing frame %d...\n", encoder->frameCount);

    size_t stride = encoder->channels * encoder->width;
    stride += (stride % 4) ? (4 - stride % 4) : 0;

    int length;
    qoi_desc desc = {
        .width = encoder->width, .height = encoder->height,
        .channels = encoder->channels, .colorspace = QOI_SRGB,
    };

    char fileName[1024]; // Should be long enough
    sprintf(fileName, "./frames/%05d.qoi", encoder->frameCount);

    qoi_write(fileName, encoder->data, &desc);

    // void *data = qoi_encode(encoder->data, &desc, &length);

    // fwrite(&length, sizeof(int), 1, encoder->outFile);
    // fwrite(data, 1, length, encoder->outFile);

    if (encoder->frameCount % 60 == 0)
    {
        // fflush(encoder->outFile);
    }

    // free(data);
}