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

static void encode(AVFormatContext* formatCtx, AVCodecContext* codecCtx, AVFrame* frame, AVPacket* pkt)
{
    /* send the frame for encoding */
    AVCHECK(avcodec_send_frame(codecCtx, frame), "Error sending the frame to the encoder")

        /* read all the available output packets (in general there may be any number of them) */
        while (true) {
            i32 result = avcodec_receive_packet(codecCtx, pkt);

            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
                return;

            if (result < 0) {
                char errbuf[256];
                av_strerror(result, errbuf, 256);
                ERROR("Error encoding audio frame: %s", errbuf);
            }

            // fwrite(pkt->data, 1, pkt->size, output);
            av_interleaved_write_frame(formatCtx, pkt);
            av_packet_unref(pkt);
        }
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
        ctx->bit_rate = 400000;
        ctx->width = 352;
        ctx->height = 288;

        outStream->stream->time_base = (AVRational){ .num = 1, .den = 60 };
        ctx->time_base = outStream->stream->time_base;
        ctx->gop_size = 12;
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        break;
    case AVMEDIA_TYPE_AUDIO:
        // ctx->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        ctx->sample_fmt = AV_SAMPLE_FMT_S16;
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
    av_frame_free(&outStream->frame);
    av_packet_free(&outStream->packet);
}

static void open_video(output_stream_t* outStream, AVFormatContext* formatCtx, const AVCodec* codec)
{
    AVCodecContext* ctx = outStream->codec_ctx;

    AVCHECK(avcodec_open2(ctx, codec, NULL), "Could not open video codec");
    outStream->frame = av_frame_alloc();
    outStream->frame->format = ctx->pix_fmt;
    outStream->frame->width = ctx->width;
    outStream->frame->height = ctx->height;
    outStream->frame_sample_pos = 0;

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

    outStream->frame = av_frame_alloc();
    outStream->frame->format = ctx->sample_fmt;
    outStream->frame->sample_rate = ctx->sample_rate;
    outStream->frame->nb_samples = sampleCount;
    AVCHECK(av_channel_layout_copy(&outStream->frame->ch_layout, &ctx->ch_layout), "Failed copying channel layout")
    outStream->frame_sample_pos = 0;

    AVCHECK(av_frame_get_buffer(outStream->frame, 0), "Failed allocating audio buffer")
    AVCHECK(avcodec_parameters_from_context(outStream->stream->codecpar, ctx), "Failed copying parameters from context")
}

static void write_frame(output_stream_t* outStream, AVFormatContext* formatCtx)
{
    /* send the frame for encoding */
    AVCHECK(avcodec_send_frame(outStream->codec_ctx, outStream->frame), "Error sending the frame to the encoder")

    /* read all the available output packets (in general there may be any number of them) */
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

    encoder->has_video = false;
    encoder->has_audio = false;
    // if (encoder->format_ctx->oformat->video_codec != AV_CODEC_ID_NONE)
    // {
    //     add_stream(&encoder->video_stream, encoder->format_ctx, &encoder->video_codec, encoder->format_ctx->oformat->video_codec);
    //     encoder->has_video = true;
    // }
    if (encoder->format_ctx->oformat->audio_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&encoder->audio_stream, encoder->format_ctx, &encoder->audio_codec, encoder->format_ctx->oformat->audio_codec);
        encoder->has_audio = true;
    }

    if (encoder->has_video)
        open_video(&encoder->video_stream, encoder->format_ctx, encoder->video_codec);
    if (encoder->has_audio)
        open_audio(&encoder->audio_stream, encoder->format_ctx, encoder->audio_codec);

    if (!(encoder->format_ctx->oformat->flags & AVFMT_NOFILE))
        AVCHECK(avio_open(&encoder->format_ctx->pb, outputFile, AVIO_FLAG_WRITE), "Failed opening output file")

    AVCHECK(avformat_write_header(encoder->format_ctx, NULL), "Failed writing header to output file")
}

void encoder_ffmpeg_destroy(encoder_ffmpeg_t* encoder)
{
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

}

void encoder_ffmpeg_prepare_sound(encoder_ffmpeg_t* encoder, u32 channelCount, size_t sampleCount, encoder_sound_format_t format)
{
    INFO("encoder_ffmpeg_prepare_sound(%i, %i, %i)", channelCount, sampleCount, format);

    size_t srcVarSize = get_sound_format_size(format);
    size_t dstVarSize = get_sound_format_size(encoder->sound_format);

    encoder->sound_data_size = max(channelCount, encoder->sound_channels) * max(srcVarSize, dstVarSize) * sampleCount;
    encoder->sound_data_samples = sampleCount;
    encoder->sound_data_channels = channelCount;
    encoder->sound_data_format = format;

    if (encoder->sound_data_buffer_size < encoder->sound_data_size)
    {
        INFO("Allocing: %i", encoder->sound_data_buffer_size);
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

}

void encoder_ffmpeg_flush_sound(encoder_ffmpeg_t* encoder)
{
    output_stream_t* outStream = &encoder->audio_stream;
    AVCodecContext* ctx = outStream->codec_ctx;

    INFO("1: %p", outStream->frame);
    INFO("2: %p", outStream->frame->data);
    INFO("3: %p", outStream->frame->data[0]);
    INFO("4: %i", ctx->sample_fmt);

    f32* srcData = (f32*)encoder->sound_data;
    i16* dstData = (i16*)outStream->frame->data[0];

    for (int s = 0; s < encoder->sound_data_samples; s++)
    {
        // TRACE("Sample: %i", s);
        for (int c = 0; c < encoder->sound_data_channels && c < ctx->ch_layout.nb_channels; c++)
        {
            // TRACE("  Channel: %i", c);
            // outStream->frame->data[0][outStream->frame_sample_pos * ctx->ch_layout.nb_channels + c]
            //     = encoder->sound_data[s * encoder->sound_data_channels + c];
            // TRACE("Idx: %i", outStream->frame_sample_pos * ctx->ch_layout.nb_channels + c);
            dstData[outStream->frame_sample_pos * ctx->ch_layout.nb_channels + c] = srcData[s * encoder->sound_data_channels + c] * 32767;
        }

        outStream->frame_sample_pos++;
        if (outStream->frame_sample_pos >= outStream->frame->nb_samples)
        {
            // This frame is completely filled with data
            AVCHECK(av_frame_make_writable(outStream->frame), "Failed making frame writable")
            outStream->frame->pts = av_rescale_q(outStream->samples_count, (AVRational){ .num = 1, .den = ctx->sample_rate }, ctx->time_base);
            outStream->samples_count += outStream->frame->nb_samples;
            outStream->frame_sample_pos = 0;

            write_frame(outStream, encoder->format_ctx);
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
