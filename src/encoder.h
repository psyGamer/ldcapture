#pragma once

typedef enum encoder_type_t
{
    ENCODER_TYPE_QOI,
} encoder_type_t;

typedef struct encoder_t
{
    encoder_type_t type;

    unsigned int channels;
    unsigned int width, height;
    unsigned char *data;

    unsigned int frameCount;
} encoder_t;

encoder_t *encoder_create(encoder_type_t type);
void encoder_destroy(encoder_t *encoder);

void encoder_resize(encoder_t *encoder, unsigned int width, unsigned int height);
void encoder_save_frame(encoder_t *encoder);
