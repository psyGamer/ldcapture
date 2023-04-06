#include <libavutil/opt.h>

#include "encoder.h"
#include "encoder_ffmpeg.h"
#include "util/math.h"

#define AVCHECK(fn, errMsg) {                 \
        i32 result = fn ;                     \
        if (result < 0) {                     \
            char errbuf[256];                 \
            av_strerror(result, errbuf, 256); \
            ERROR(errMsg ": %s", errbuf);     \
        }                                     \
    }

static void add_stream(output_stream_t* outStream, AVFormatContext* formatCtx, const AVCodec** codec, enum AVCodecID codecId)
{
    *codec = avcodec_find_encoder(codecId);

    outStream->packet = av_packet_alloc();
    outStream->stream = avformat_new_stream(formatCtx, NULL);
    outStream->stream->id = formatCtx->nb_streams - 1;
    outStream->codec_ctx = avcodec_alloc_context3(*codec);

    AVCodecContext* ctx = outStream->codec_ctx;

    switch ((*codec)->type)
    {
    case AVMEDIA_TYPE_VIDEO:
        ctx->codec_id = codecId;
        ctx->bit_rate = 1000000;
        ctx->width = 1920;
        ctx->height = 1080;

        outStream->stream->time_base = (AVRational){ .num = 1, .den = 60 };
        ctx->time_base = outStream->stream->time_base;
        ctx->gop_size = 12;
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        break;
    case AVMEDIA_TYPE_AUDIO:
        ctx->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        // ctx->sample_fmt = AV_SAMPLE_FMT_S16;
        ctx->bit_rate = 64000;
        ctx->sample_rate = 44100;

        if ((*codec)->supported_samplerates)
        {
            ctx->sample_rate = (*codec)->supported_samplerates[0];
            for (i32 i = 0; (*codec)->supported_samplerates[i]; i++)
            {
                if ((*codec)->supported_samplerates[i] == 44100)
                    ctx->sample_rate = 44100;
            }
        }

        ctx->sample_rate = 48000;

        AVCHECK(av_channel_layout_copy(&ctx->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO), "Failed copying channel layout")
            outStream->stream->time_base = (AVRational){ .num = 1, .den = ctx->sample_rate };
    default:
        break;
    }

    if (formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

static void close_stream(output_stream_t* outStream, AVFormatContext* formatCtx)
{
    avcodec_free_context(&outStream->codec_ctx);
    av_frame_free(&outStream->in_frame);
    av_frame_free(&outStream->out_frame);
    av_packet_free(&outStream->packet);
}

static AVFrame* alloc_video_frame(enum AVPixelFormat pixFmt, i32 width, i32 height)
{
    AVFrame* frame = av_frame_alloc();
    frame->format = pixFmt;
    frame->width = width;
    frame->height = height;

    AVCHECK(av_frame_get_buffer(frame, 0), "Failed allocating frame buffer")

    return frame;
}

static AVFrame* alloc_audio_frame(enum AVSampleFormat sampleFmt, AVChannelLayout* chLayout, i32 sampleRate, i32 sampleCount)
{
    AVFrame* frame = av_frame_alloc();
    frame->format = sampleFmt;
    frame->sample_rate = sampleRate;
    frame->nb_samples = sampleCount;

    AVCHECK(av_channel_layout_copy(&frame->ch_layout, chLayout), "Failed copying channel layout")
    AVCHECK(av_frame_get_buffer(frame, 0), "Failed allocating frame buffer")

    return frame;
}

static void open_video(output_stream_t* outStream, AVFormatContext* formatCtx, const AVCodec* codec)
{
    AVCodecContext* ctx = outStream->codec_ctx;

    AVCHECK(avcodec_open2(ctx, codec, NULL), "Could not open video codec");
    outStream->in_frame = alloc_video_frame(AV_PIX_FMT_RGBA, ctx->width, ctx->height);
    outStream->out_frame = alloc_video_frame(ctx->pix_fmt, ctx->width, ctx->height);
    outStream->frame_sample_pos = 0;

    outStream->sws_ctx = sws_getContext(ctx->width, ctx->height, AV_PIX_FMT_RGBA,
                                        ctx->width, ctx->height, ctx->pix_fmt,
                                        SWS_BICUBIC, NULL, NULL, NULL);

    AVCHECK(avcodec_parameters_from_context(outStream->stream->codecpar, ctx), "Failed copying parameters from context")
}

static void open_audio(output_stream_t* outStream, AVFormatContext* formatCtx, const AVCodec* codec)
{
    AVCodecContext* ctx = outStream->codec_ctx;

    AVCHECK(avcodec_open2(ctx, codec, NULL), "Could not open audio codec");

    i32 sampleCount;
    if (ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        sampleCount = 10000;
    else
        sampleCount = ctx->frame_size;

    outStream->in_frame  = alloc_audio_frame(AV_SAMPLE_FMT_FLT, &ctx->ch_layout, ctx->sample_rate, sampleCount);
    outStream->out_frame = alloc_audio_frame(ctx->sample_fmt, &ctx->ch_layout, ctx->sample_rate, sampleCount);
    outStream->frame_count = 0;

    outStream->swr_ctx = swr_alloc();
    av_opt_set_chlayout  (outStream->swr_ctx, "in_chlayout",     &ctx->ch_layout,    0);
    av_opt_set_int       (outStream->swr_ctx, "in_sample_rate",   ctx->sample_rate,  0);
    av_opt_set_sample_fmt(outStream->swr_ctx, "in_sample_fmt",    AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_chlayout  (outStream->swr_ctx, "out_chlayout",    &ctx->ch_layout,    0);
    av_opt_set_int       (outStream->swr_ctx, "out_sample_rate",  ctx->sample_rate,  0);
    av_opt_set_sample_fmt(outStream->swr_ctx, "out_sample_fmt",   ctx->sample_fmt,   0);
    AVCHECK(swr_init(outStream->swr_ctx), "Failed initializing resampling context")

    AVCHECK(avcodec_parameters_from_context(outStream->stream->codecpar, ctx), "Failed copying parameters from context")
}

static void write_frame(output_stream_t* outStream, AVFormatContext* formatCtx, AVFrame* frame)
{
    // Send the frame for encoding
    AVCHECK(avcodec_send_frame(outStream->codec_ctx, frame), "Error sending the frame to the encoder")

    // Read all the available output packets (in general there may be any number of them)
    while (true) {
        i32 result = avcodec_receive_packet(outStream->codec_ctx, outStream->packet);

        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            return;

        if (result < 0) {
            char errbuf[256];
            av_strerror(result, errbuf, 256);
            ERROR("Error encoding audio frame: %s", errbuf);
        }

        av_packet_rescale_ts(outStream->packet, outStream->codec_ctx->time_base, outStream->stream->time_base);
        outStream->packet->stream_index = outStream->stream->index;
        AVCHECK(av_interleaved_write_frame(formatCtx, outStream->packet), "Failed writing output packet")
    }
}

void encoder_ffmpeg_create(encoder_ffmpeg_t* encoder)
{
    const char* outputFile = "ffmpeg-encode.mkv";

    AVCHECK(avformat_alloc_output_context2(&encoder->format_ctx, NULL, NULL, outputFile), "Failed allocating format context");
    INFO("VIDEO CODEC: %s", avcodec_get_name(encoder->format_ctx->oformat->video_codec));
    INFO("AUDIO CODEC: %s", avcodec_get_name(encoder->format_ctx->oformat->audio_codec));

    const AVCodec* video_codec;
    const AVCodec* audio_codec;
    encoder->has_video = false;
    encoder->has_audio = false;
    if (encoder->format_ctx->oformat->video_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&encoder->video_stream, encoder->format_ctx, &video_codec, encoder->format_ctx->oformat->video_codec);
        encoder->has_video = true;
    }
    if (encoder->format_ctx->oformat->audio_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&encoder->audio_stream, encoder->format_ctx, &audio_codec, encoder->format_ctx->oformat->audio_codec);
        encoder->has_audio = true;
    }

    if (encoder->has_video)
        open_video(&encoder->video_stream, encoder->format_ctx, video_codec);
    if (encoder->has_audio)
        open_audio(&encoder->audio_stream, encoder->format_ctx, audio_codec);

    if (!(encoder->format_ctx->oformat->flags & AVFMT_NOFILE))
        AVCHECK(avio_open(&encoder->format_ctx->pb, outputFile, AVIO_FLAG_WRITE), "Failed opening output file")

    AVCHECK(avformat_write_header(encoder->format_ctx, NULL), "Failed writing header to output file")
}

void encoder_ffmpeg_destroy(encoder_ffmpeg_t* encoder)
{
    // Flush the encoders
    if (encoder->has_video)
        write_frame(&encoder->video_stream, encoder->format_ctx, NULL);
    if (encoder->has_audio)
        write_frame(&encoder->audio_stream, encoder->format_ctx, NULL);

    av_write_trailer(encoder->format_ctx);

    if (encoder->has_video)
        close_stream(&encoder->video_stream, encoder->format_ctx);
    if (encoder->has_audio)
        close_stream(&encoder->audio_stream, encoder->format_ctx);

    if (!(encoder->format_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&encoder->format_ctx->pb);

    avformat_free_context(encoder->format_ctx);
}

void encoder_ffmpeg_prepare_video(encoder_ffmpeg_t* encoder, u32 width, u32 height)
{
    output_stream_t* outStream = &encoder->video_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    if (outStream->in_frame->width != width ||
        outStream->in_frame->height != height)
    {
        av_frame_free(&outStream->in_frame);
        outStream->in_frame = alloc_video_frame(AV_PIX_FMT_RGBA, width, height);

        sws_freeContext(outStream->sws_ctx);
        outStream->sws_ctx = sws_getContext(
            width, height, AV_PIX_FMT_RGBA,
            ctx->width, ctx->height, ctx->pix_fmt,
            SWS_BICUBIC, NULL, NULL, NULL);

        encoder->video_row_stride = outStream->in_frame->linesize[0];
    }

    encoder->video_data = outStream->in_frame->data[0];

//     bool reallocate = (encoder->video_width * encoder->video_height) < (width * height) ||   // Buffer too small
//                       (encoder->video_width * encoder->video_height) > (width * height * 2); // Buffer is more than double as big as it should be
    
//     encoder->video_width = width;
//     encoder->video_height = height;

//     if (reallocate)
//     {
//         INFO("Allocing: %i", width * height * encoder->video_channels);
//         encoder->video_data = realloc(encoder->video_data, width * height * encoder->video_channels);

//         if (encoder->video_data == NULL)
//         {
//             FATAL("Failed to allocate video buffer!");
//             exit(1);
//         }
//     }
}

void encoder_ffmpeg_prepare_sound(encoder_ffmpeg_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format)
{
    // INFO("encoder_ffmpeg_prepare_sound(%i, %i, %i)", channelCount, sampleCount, format);

    size_t srcVarSize = get_sound_format_size(format);
    size_t dstVarSize = get_sound_format_size(encoder->sound_format);

    encoder->sound_data_size = max(channelCount, encoder->sound_channels) * max(srcVarSize, dstVarSize) * sampleCount;
    encoder->sound_data_samples = sampleCount;
    encoder->sound_data_channels = channelCount;
    encoder->sound_data_format = format;

    if (encoder->sound_data_buffer_size < encoder->sound_data_size)
    {
        encoder->sound_data_buffer_size = encoder->sound_data_size * 2; // Avoid having to reallocate soon
        INFO("Allocing: %i", encoder->sound_data_buffer_size);
        encoder->sound_data = realloc(encoder->sound_data, encoder->sound_data_buffer_size);

        if (encoder->sound_data == NULL)
        {
            FATAL("Failed to allocate sound buffer!");
            exit(1);
        }
    }
}
 
/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index,
                           int width, int height)
{
    int x, y, i;
 
    i = frame_index;
 
    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
 
    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

#include "api.h"

void encoder_ffmpeg_flush_video(encoder_ffmpeg_t* encoder)
{
    output_stream_t* outStream = &encoder->video_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    // memcpy(outStream->in_frame->data[0], encoder->video_data, ctx->width * ctx->height * 4);
    // for (i32 x = 0; x < ctx->width; x++)
    // {
    //     for (i32 y = 0; y < ctx->height; y++)
    //     {
    //         outStream->in_frame->data[0][y * outStream->in_frame->linesize[0] + x + 0] = 255;
    //         outStream->in_frame->data[0][y * outStream->in_frame->linesize[0] + x + 1] = 255;
    //         outStream->in_frame->data[0][y * outStream->in_frame->linesize[0] + x + 2] = 255;
    //         outStream->in_frame->data[0][y * outStream->in_frame->linesize[0] + x + 3] = 255;
    //     }
    // }

    // for (i32 y = 0; y < ctx->height; y++)
    // {
    //     for (i32 x = 0; x < ctx->width; x++)
    //     {
    //         i32 dstIdx = (y * ctx->width + x) * 4;

    //         if (x >= encoder->video_width || y >= encoder->video_height)
    //         {
    //             outStream->in_frame->data[0][dstIdx + 0] = 0;
    //             outStream->in_frame->data[0][dstIdx + 1] = 0;
    //             outStream->in_frame->data[0][dstIdx + 2] = 0;
    //             outStream->in_frame->data[0][dstIdx + 3] = 255;
    //         }
    //         else
    //         {
    //             i32 srcIdx = (y * encoder->video_width + x) * 4;

    //             outStream->in_frame->data[0][dstIdx + 0] = encoder->video_data[srcIdx + 0];
    //             outStream->in_frame->data[0][dstIdx + 1] = encoder->video_data[srcIdx + 1];
    //             outStream->in_frame->data[0][dstIdx + 2] = encoder->video_data[srcIdx + 2];
    //             outStream->in_frame->data[0][dstIdx + 3] = encoder->video_data[srcIdx + 3];
    //         }
    //     }
        
    // }
    // memset(outStream->in_frame->data[0], 255, ctx->width * ctx->height * 4);

    AVCHECK(av_frame_make_writable(outStream->in_frame), "Failed making frame writable")
    AVCHECK(sws_scale(outStream->sws_ctx, (const u8* const*)outStream->in_frame->data,  outStream->in_frame->linesize, 0, outStream->in_frame->height,
                                                    outStream->out_frame->data, outStream->out_frame->linesize),
            "Failed resampling video");
    // fill_yuv_image(outStream->out_frame, outStream->frame_count, ctx->width, ctx->height);

    outStream->out_frame->pts = outStream->frame_count++;
    write_frame(outStream, encoder->format_ctx, outStream->out_frame);

    // if (outStream->frame_count >= 60)
    //     ldcapture_StopRecording();
}

void encoder_ffmpeg_flush_sound(encoder_ffmpeg_t* encoder)
{
    output_stream_t* outStream = &encoder->audio_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    // INFO("1: %p", outStream->frame);
    // INFO("2: %p", outStream->frame->data);
    // INFO("3: %p", outStream->frame->data[0]);
    // INFO("4: %i", ctx->sample_fmt);

    f32* srcData = (f32*)encoder->sound_data;
    f32* dstData = (f32*)outStream->in_frame->data[0];

    for (int s = 0; s < encoder->sound_data_samples; s++)
    {
        // TRACE("Sample: %i", s);
        for (int c = 0; c < encoder->sound_data_channels && c < ctx->ch_layout.nb_channels; c++)
        {
            // TRACE("  Channel: %i", c);
            // outStream->frame->data[0][outStream->frame_sample_pos * ctx->ch_layout.nb_channels + c]
            //     = encoder->sound_data[s * encoder->sound_data_channels + c];
            // TRACE("Idx: %i", outStream->frame_sample_pos * ctx->ch_layout.nb_channels + c);
            dstData[outStream->frame_sample_pos * ctx->ch_layout.nb_channels + c] = srcData[s * encoder->sound_data_channels + c];
        }

        outStream->frame_sample_pos++;
        if (outStream->frame_sample_pos >= outStream->in_frame->nb_samples)
        {
            // This frame is completely filled with data

            i32 dstSampleCount = av_rescale_rnd(swr_get_delay(outStream->swr_ctx, ctx->sample_rate) + outStream->in_frame->nb_samples,
                                                ctx->sample_rate, ctx->sample_rate, AV_ROUND_UP);

            AVCHECK(av_frame_make_writable(outStream->out_frame), "Failed making frame writable")
            AVCHECK(swr_convert(outStream->swr_ctx, outStream->out_frame->data, dstSampleCount, 
                                        (const u8**)outStream->in_frame->data,  outStream->in_frame->nb_samples),
                    "Failed resampling audio data")
            
            outStream->out_frame->pts = av_rescale_q(outStream->samples_count, (AVRational){ .num = 1, .den = ctx->sample_rate }, ctx->time_base);
            outStream->samples_count += outStream->out_frame->nb_samples;
            outStream->frame_sample_pos = 0;
            // INFO("PTS: %i", outStream->out_frame->pts);
            write_frame(outStream, encoder->format_ctx, outStream->out_frame);
        }
    }
    // AVCHECK(av_frame_make_writable(encoder->frame), "Failed making frame writable");
    // uint16_t* samples = (uint16_t*)encoder->frame->data[0];

    // for (i32 j = 0; j < encoder->codec_ctx->frame_size; j++) {
    //     samples[2 * j] = 25000;

    //     for (i32 k = 1; k < encoder->codec_ctx->ch_layout.nb_channels; k++)
    //         samples[2 * j + k] = samples[2 * j];
    // }

    // encode(encoder->format_ctx, encoder->codec_ctx, encoder->frame, encoder->packet);
    
}
