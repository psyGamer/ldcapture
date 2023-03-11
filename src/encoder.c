#include "encoder.h"
#include "encoder_qoi.h"

encoder_t *encoder_create(encoder_type_t type)
{
    encoder_t *encoder = malloc(sizeof(encoder_t));
    encoder->type = type;
    encoder->channels = 4; // Currently hardcoded
    encoder->width = 0;
    encoder->height = 0;
    encoder->data = NULL;
    encoder->frame_count = 0;

    switch (type)
    {
    case ENCODER_TYPE_QOI:
        encoder_qoi_create((encoder_qoi_t *)encoder);
        break;
    }

    return encoder;
}

void encoder_destroy(encoder_t *encoder)
{
    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI:
        encoder_qoi_destory((encoder_qoi_t *)encoder);
        break;
    }

    free(encoder->data);
    free(encoder);
}

void encoder_resize(encoder_t *encoder, u32 width, u32 height)
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
    if (encoder->frame_count <= 10)
    {
        // The first few frames are usually garbage.
        encoder->frame_count++;
        return;
    }

    switch (encoder->type)
    {
    case ENCODER_TYPE_QOI:
        encoder_qoi_save_frame((encoder_qoi_t *)encoder);
        break;
    }

    encoder->frame_count++;
}