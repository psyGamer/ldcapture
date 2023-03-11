#pragma once

#include "base.h"

typedef enum encoder_type_t
{
    ENCODER_TYPE_QOI,
} encoder_type_t;

typedef struct encoder_t
{
    encoder_type_t type;

    u32 channels;
    u32 width, height;
    u8 *data;

    u32 frame_count;
} encoder_t;

encoder_t *encoder_create(encoder_type_t type);
void encoder_destroy(encoder_t *encoder);

void encoder_resize(encoder_t *encoder, u32 width, u32 height);
void encoder_save_frame(encoder_t *encoder);
