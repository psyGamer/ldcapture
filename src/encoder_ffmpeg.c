#include <libavutil/opt.h>

#include "settings.h"
#include "encoder.h"
#include "encoder_ffmpeg.h"
#include "filesystem.h"
#include "util/math.h"

#define AVCHECK(fn, errMsg) {                 \
        i32 result = fn ;                     \
        if (result < 0) {                     \
            char errbuf[256];                 \
            av_strerror(result, errbuf, 256); \
            ERROR(errMsg ": %s", errbuf);     \
        }                                     \
    }

static void add_stream(encoder_ffmpeg_t* encoder, output_stream_t* outStream, const AVCodec* codec, enum AVCodecID codecId)
{
    outStream->packet = av_packet_alloc();
    outStream->stream = avformat_new_stream(encoder->format_ctx, NULL);
    outStream->stream->id = encoder->format_ctx->nb_streams - 1;
    outStream->codec_ctx = avcodec_alloc_context3(codec);

    AVCodecContext* ctx = outStream->codec_ctx;

    switch (codec->type)
    {
    case AVMEDIA_TYPE_VIDEO:
        ctx->codec_id = codecId;
        ctx->bit_rate = settings_video_bitrate();
        ctx->width = settings_video_width();
        ctx->height = settings_video_height();
        ctx->time_base = (AVRational){ 1, settings_fps() * 10000 };
        ctx->framerate = (AVRational){ settings_fps(), 1 };
        ctx->gop_size = 12;
        ctx->pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;

        outStream->stream->time_base = ctx->time_base;
        break;
    case AVMEDIA_TYPE_AUDIO:
        ctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        ctx->bit_rate = settings_audio_bitrate();
        ctx->sample_rate = settings_audio_samplerate();

        AVCHECK(av_channel_layout_copy(&ctx->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO), "Failed copying channel layout");
        outStream->stream->time_base = (AVRational){ .num = 1, .den = ctx->sample_rate };
    default:
        break;
    }

    if (encoder->format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
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

static void open_video(encoder_ffmpeg_t* encoder, const AVCodec* codec)
{
    output_stream_t* outStream = &encoder->video_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    i32 codecOptionsLength = settings_video_codec_options_length();

    if (codecOptionsLength > 0)
    {
        codec_option_t* codecOptions = settings_video_codec_options();
        AVDictionary* options = { 0 };
        for (i32 i = 0; i < codecOptionsLength; i++)
        {
            codec_option_t* option = &codecOptions[i];
            av_dict_set(&options, option->name, option->value, 0);
        }

        AVCHECK(avcodec_open2(ctx, codec, &options), "Could not open video codec");
    }
    else
    {
        AVCHECK(avcodec_open2(ctx, codec, NULL), "Could not open video codec");
    }

    outStream->in_frame = alloc_video_frame(AV_PIX_FMT_RGBA, ctx->width, ctx->height);
    outStream->out_frame = alloc_video_frame(ctx->pix_fmt, ctx->width, ctx->height);
    outStream->frame_sample_pos = 0;
    encoder->video_row_stride = outStream->in_frame->linesize[0];

    outStream->sws_ctx = sws_getContext(ctx->width, ctx->height, AV_PIX_FMT_RGBA,
                                        ctx->width, ctx->height, ctx->pix_fmt,
                                        SWS_BICUBIC, NULL, NULL, NULL);

    AVCHECK(avcodec_parameters_from_context(outStream->stream->codecpar, ctx), "Failed copying parameters from context")
}

static void open_audio(encoder_ffmpeg_t* encoder, const AVCodec* codec)
{
    output_stream_t* outStream = &encoder->audio_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    i32 codecOptionsLength = settings_audio_codec_options_length();

    if (codecOptionsLength > 0)
    {
        codec_option_t* codecOptions = settings_audio_codec_options();
        AVDictionary* options = { 0 };
        for (i32 i = 0; i < codecOptionsLength; i++)
        {
            codec_option_t* option = &codecOptions[i];
            av_dict_set(&options, option->name, option->value, 0);
        }

        AVCHECK(avcodec_open2(ctx, codec, &options), "Could not open audio codec");
    }
    else
    {
        AVCHECK(avcodec_open2(ctx, codec, NULL), "Could not open audio codec");
    }

    i32 sampleCount = ctx->frame_size;
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

        if (frame)
            outStream->packet->duration = frame->duration;

        av_packet_rescale_ts(outStream->packet, outStream->codec_ctx->time_base, outStream->stream->time_base);
        outStream->packet->stream_index = outStream->stream->index;
        AVCHECK(av_interleaved_write_frame(formatCtx, outStream->packet), "Failed writing output packet")
    }
}

void encoder_ffmpeg_create(encoder_ffmpeg_t* encoder)
{
    encoder->sound_data_channels = 0;
    encoder->sound_data_samples = 0;
    encoder->sound_data_format = 0;
    encoder->sound_data_size = 0;
    encoder->sound_data_buffer_size = 0;
    encoder->format_ctx = 0;
    encoder->has_video = false;
    encoder->has_audio = false;
    encoder->video_stream = (output_stream_t){0};
    encoder->audio_stream = (output_stream_t){0};

    INFO("%s.%s", encoder->save_directory, settings_container_type());
    char outputFile[1024];
    sprintf(outputFile, "%s.%s", encoder->save_directory, settings_container_type());

    AVCHECK(avformat_alloc_output_context2(&encoder->format_ctx, NULL, NULL, outputFile), "Failed allocating format context");

    enum AVCodecID video_codec_id = encoder->format_ctx->oformat->video_codec;
    enum AVCodecID audio_codec_id = encoder->format_ctx->oformat->audio_codec;

    const AVCodec* video_codec = NULL;
    if (settings_overwrite_video_codec())
        video_codec = avcodec_find_encoder_by_name(settings_video_codec());
    else if (video_codec_id != AV_CODEC_ID_NONE)
        video_codec = avcodec_find_encoder(video_codec_id);

    const AVCodec* audio_codec = NULL;
    if (settings_overwrite_audio_codec())
        audio_codec = avcodec_find_encoder_by_name(settings_audio_codec());
    else if (video_codec_id != AV_CODEC_ID_NONE)
        audio_codec = avcodec_find_encoder(audio_codec_id);

    encoder->has_video = video_codec != NULL;
    encoder->has_audio = audio_codec != NULL;

    if (encoder->has_video)
        add_stream(encoder, &encoder->video_stream, video_codec, video_codec_id);
    if (encoder->has_audio)
        add_stream(encoder, &encoder->audio_stream, audio_codec, audio_codec_id);

    if (encoder->has_video)
        open_video(encoder, video_codec);
    if (encoder->has_audio)
        open_audio(encoder, audio_codec);

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
}

void encoder_ffmpeg_prepare_sound(encoder_ffmpeg_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format)
{
    size_t srcVarSize = get_sound_format_size(format);
    size_t dstVarSize = get_sound_format_size(encoder->sound_format);

    encoder->sound_data_size = max(channelCount, encoder->sound_channels) * max(srcVarSize, dstVarSize) * sampleCount;
    encoder->sound_data_samples = sampleCount;
    encoder->sound_data_channels = channelCount;
    encoder->sound_data_format = format;

    if (encoder->sound_data_buffer_size < encoder->sound_data_size)
    {
        encoder->sound_data_buffer_size = encoder->sound_data_size * 2; // Avoid having to reallocate soon
        encoder->sound_data = realloc(encoder->sound_data, encoder->sound_data_buffer_size);

        if (encoder->sound_data == NULL)
        {
            FATAL("Failed to allocate sound buffer!");
            exit(1);
        }
    }
}

void encoder_ffmpeg_flush_video(encoder_ffmpeg_t* encoder)
{
    output_stream_t* outStream = &encoder->video_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    AVCHECK(av_frame_make_writable(outStream->in_frame), "Failed making frame writable")
    AVCHECK(sws_scale(outStream->sws_ctx, (const u8* const*)outStream->in_frame->data,  outStream->in_frame->linesize, 0, outStream->in_frame->height,
                                                            outStream->out_frame->data, outStream->out_frame->linesize),
            "Failed resampling video");

    outStream->out_frame->duration = ctx->time_base.den / ctx->time_base.num / ctx->framerate.num * ctx->framerate.den;
    outStream->out_frame->pts = outStream->frame_count++ * outStream->out_frame->duration;

    write_frame(outStream, encoder->format_ctx, outStream->out_frame);
}

void encoder_ffmpeg_flush_sound(encoder_ffmpeg_t* encoder)
{
    output_stream_t* outStream = &encoder->audio_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    f32* srcData = (f32*)encoder->sound_data;
    f32* dstData = (f32*)outStream->in_frame->data[0];

    for (int s = 0; s < encoder->sound_data_samples; s++)
    {
        for (int c = 0; c < encoder->sound_data_channels && c < ctx->ch_layout.nb_channels; c++)
        {
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

            write_frame(outStream, encoder->format_ctx, outStream->out_frame);
        }
    }
}
