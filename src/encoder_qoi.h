#pragma once

#include "base.h"
#include "encoder.h"

typedef struct encoder_qoi_t
{
    encoder_type_t type;

    u32 channels;
    u32 width, height;
    u8* data;

    u32 frame_count;

    FILE* out_file;
} encoder_qoi_t;

void encoder_qoi_create(encoder_qoi_t* encoder);
void encoder_qoi_destory(encoder_qoi_t* encoder);

void encoder_qoi_save_frame(encoder_qoi_t* encoder);