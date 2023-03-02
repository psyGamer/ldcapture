#include "encoder.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static void encoder_png_save_frame(encoder_t *encoder);

encoder_t *encoder_create(encoder_type_t type)
{
    encoder_t *encoder = malloc(sizeof(encoder_t));
    encoder->type = type;
    encoder->channels = 3; // Currently hardcoded
    encoder->width = 0;
    encoder->height = 0;
    encoder->data = NULL;
    encoder->frameCount = 0;
    return encoder;
}

void encoder_destroy(encoder_t *encoder)
{
    free(encoder->data);
    free(encoder);
}

void encoder_resize(encoder_t *encoder, unsigned int width, unsigned int height)
{
    bool reallocate = (encoder->width * encoder->height) < (width * height) ||   // Buffer too small
                      (encoder->width * encoder->height) > (width * height * 2); // Buffer is more than double as big as it should be

    reallocate = true;

    encoder->width = width;
    encoder->height = height;

    if (reallocate)
    {
        free(encoder->data);
        // 4 Channels because glReadPixels aligns every pixel by 4
        encoder->data = malloc(width * height * 4);
    }
}

void encoder_save_frame(encoder_t *encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_PNG:
        encoder_png_save_frame(encoder);
    }

    encoder->frameCount++;
}

static void encoder_png_save_frame(encoder_t *encoder)
{
    // Create "frames" directory if doesn't exist
    struct stat st = {0};
    if (stat("./frames", &st) == -1) mkdir("./frames", 0700);

    size_t stride = encoder->channels * encoder->width;
    stride += (stride % 4) ? (4 - stride % 4) : 0;

    char fileName[1024]; // Should be long enough
    sprintf(fileName, "./frames/%05d.png", encoder->frameCount);

    printf("Writing frame %d: %s...\n", encoder->frameCount, fileName);

    stbi_flip_vertically_on_write(true);
    stbi_write_png(fileName, encoder->width, encoder->height, encoder->channels, encoder->data, stride);
    // stbi_write_png("output.png", encoder->width, encoder->height, encoder->channels, encoder->data, stride);
}