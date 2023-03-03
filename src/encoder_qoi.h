#pragma once

#include <stdio.h>

#include "encoder.h"

typedef struct encoder_qoi_t
{
    encoder_type_t type;

    unsigned int channels;
    unsigned int width, height;
    unsigned char *data;

    unsigned int frameCount;

    FILE *outFile;
} encoder_qoi_t;

void encoder_qoi_create(encoder_qoi_t *encoder);
void encoder_qoi_destory(encoder_qoi_t *encoder);

void encoder_qoi_save_frame(encoder_qoi_t *encoder);