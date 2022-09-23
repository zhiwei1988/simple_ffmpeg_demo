#include "muxer_core.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
}

#define STREAM_FRAME_RATE 25

static AVFormatContext* video_fmt_ctx = nullptr;
static AVFormatContext* audio_fmt_ctx = nullptr;
static AVFormatContext* output_fmt_ctx = nullptr;
static int32_t in_video_st_idx = -1;
static int32_t in_audio_st_idx = -1;
static int32_t out_video_st_idx = -1;
static int32_t out_audio_st_idx = -1;

static int32_t init_input_video(char* video_input_file, const char* video_format)
{
    int32_t result = 0;
    // 根据输入文件的格式名称查找 AVInputFormat 结构
    const AVInputFormat* video_input_format = av_find_input_format(video_format);
    if (video_input_format == nullptr) {
        printf("Fail to find proper AVInputFormat for format: %s\n", video_format);
        return -1;
    }

    result = avformat_open_input(&video_fmt_ctx, video_input_file, video_input_format, nullptr);
    if (result < 0) {
        printf("avformat_open_input fail\n");
        return -1;
    }

    result = avformat_find_stream_info(video_fmt_ctx, nullptr);
    if (result < 0) {
        printf("avformat_find_stream_info fail\n");
        return -1;
    }

    return result;
}

static int32_t init_input_audio(char* audio_input_file, const char* audio_format)
{
    int32_t result = 0;
    // 根据输入文件的格式名称查找 AVInputFormat 结构
    const AVInputFormat* audio_input_format = av_find_input_format(audio_format);
    if (audio_input_format == nullptr) {
        printf("Fail to find proper AVInputFormat for format: %s\n", audio_format);
        return -1;
    }

    result = avformat_open_input(&audio_fmt_ctx, audio_input_file, audio_input_format, nullptr);
    if (result < 0) {
        printf("avformat_open_input fail\n");
        return -1;
    }

    result = avformat_find_stream_info(audio_fmt_ctx, nullptr);
    if (result < 0) {
        printf("avformat_find_stream_info fail\n");
        return -1;
    }

    return result;
}

static int32_t init_output(char* output_file)
{
    int32_t result = 0;
    // 创建 AVFormatContext 结构的输出文件上下文句柄
    avformat_alloc_output_context2(&output_fmt_ctx, nullptr, nullptr, output_file);
    if (result < 0) {
        printf("alloc output format context fail\n");
        return -1;
    }

    // 在创建输出文件句柄后，接下来要向其中添加媒体流
    // 添加媒体流可以使用函数 avformat_new_stream 实现
    const AVOutputFormat* fmt = output_fmt_ctx->oformat;
    printf("Default video codec id: %d audio codec id: %d\n", fmt->video_codec, fmt->audio_codec);

    AVStream* video_stream = avformat_new_stream(output_fmt_ctx, nullptr);
    if (video_stream == nullptr) {
        printf("add video stream to output format context fail\n");
        return -1;
    }

    // 新创建的 AVStream 结构基本是空的，缺少关键信息。为了将输入媒体流和输出媒体流的参数对齐，
    // 需要将输入文件中媒体流的参数（主要是码流编码参数）复制到输出文件对应的媒体流中。
    out_video_st_idx = video_stream->index;
    in_video_st_idx = av_find_best_stream(video_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (in_video_st_idx < 0) {
        printf("find video stream in input video file failed\n");
        return -1;
    }

    result = avcodec_parameters_copy(video_stream->codecpar, video_fmt_ctx->streams[in_video_st_idx]->codecpar);
    if (result < 0) {
        printf("copy video codec paramaters failed!\n");
        return -1;
    }

    video_stream->id = output_fmt_ctx->nb_streams - 1;
    video_stream->time_base = (AVRational){1, STREAM_FRAME_RATE};

    AVStream* audio_stream = avformat_new_stream(output_fmt_ctx, nullptr);
    if (audio_stream == nullptr) {
        printf("add audio stream to output format context fail\n");
        return -1;
    }

    out_audio_st_idx = audio_stream->index;
    in_audio_st_idx = av_find_best_stream(audio_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (in_audio_st_idx < 0) {
        printf("find audio stream in input video file failed\n");
        return -1;
    }

    result = avcodec_parameters_copy(audio_stream->codecpar, audio_fmt_ctx->streams[in_audio_st_idx]->codecpar);
    if (result < 0) {
        printf("copy audio codec paramaters failed!\n");
        return -1;
    }

    audio_stream->id = output_fmt_ctx->nb_streams - 1;
    audio_stream->time_base = (AVRational){1, audio_stream->codecpar->sample_rate};

    av_dump_format(output_fmt_ctx, 0, output_file, 1);
    printf("output video idx: %d audio idx: %d\n", out_video_st_idx, out_audio_st_idx);

    // 有的输出格式没有输出文件
    if (!(fmt->flags & AVFMT_NOFILE)) {
        result = avio_open(&output_fmt_ctx->pb, output_file, AVIO_FLAG_WRITE);
        if (result < 0) {
            printf("avio_open output file fail\n");
            return -1;
        }
    }

    return result;
}

int32_t init_muxer(char* video_input_file, char* auido_input_file, char* output_file)
{
    int32_t result = init_input_video(video_input_file, "hevc");
    if (result < 0) {
        return result;
    }

    result  = init_input_audio(auido_input_file, "aac");
    if (result < 0) {
        return result;
    }

    result = init_output(output_file);
    if (result < 0) {
        return result;
    }

    return 0;
}

int32_t muxing()
{
    int32_t result = 0;
    int64_t pre_video_dts = -1;
    int64_t cur_video_pts = 0;
    int64_t cur_audio_pts = 0;

    AVStream* in_video_st = video_fmt_ctx->streams[in_video_st_idx];
    AVStream* in_audio_st = audio_fmt_ctx->streams[in_audio_st_idx];
    AVStream* output_stream = nullptr;
    AVStream* input_stream = nullptr;

    int32_t video_frame_idx = 0;
    int32_t audio_frame_idx = 0;
    result = avformat_write_header(output_fmt_ctx, nullptr);
    if (result < 0) {
        printf("avformat_write_header fail\n");
        return -1;
    }

    AVPacket *pkt = av_packet_alloc();
    pkt->data = nullptr;
    pkt->size = 0;

    printf("Video r_frame_rate: %d / %d\n", in_video_st->r_frame_rate.num, in_video_st->r_frame_rate.den);
    printf("Video time_base: %d / %d\n", in_video_st->time_base.num, in_video_st->time_base.den);
    
    while (1) {
        // av_compare_ts，其作用是根据对应的时间基比较两个时间戳的顺序。若当前已记录的音频时间戳比视频时间戳新，则从输入视频文件中读取数据并写入；
        // 反之，若当前已记录的视频时间戳比音频时间戳新，则从输入音频文件中读取数据并写入。
        if (av_compare_ts(cur_video_pts, in_video_st->time_base, cur_audio_pts, in_audio_st->time_base) <= 0) {
            input_stream = in_video_st;
            result = av_read_frame(video_fmt_ctx, pkt);
            if (result < 0) {
                printf("av_read_frame fail\n");
                av_packet_unref(pkt);
                break;
            }

            if (pkt->pts == AV_NOPTS_VALUE) {
                // 有些输入流编码格式如 H.264 裸码流
                // 从中读取的视频包中通常不包含时间戳数据，所以无法通过函数 av_compare_ts 来比较时间戳。
                // 为此，我们通过视频帧数和给定帧率计算每一个 AVPacket 结构的时间戳并为其赋值。

                // r_frame_rate 表示的是音视频流中可以精准表示所有时间戳的最低帧率。
                // 简单来说，如果当前音视频流的帧率是恒定的，那么 r_frame_rate 表示的是音视频流的实际帧率；
                // 如果当前音视频流的帧率波动较大，那么 r_frame_rate 的值通常会高于整体平均帧率，以此作为每一帧的时间戳的单位
                
                // 计算每一帧持续的时间
                int64_t frame_duration = (double)AV_TIME_BASE / av_q2d(in_video_st->r_frame_rate);

                // 将帧时长的单位转换为以 time_base 为基准
                pkt->duration = (double)frame_duration / (double)(av_q2d(in_video_st->time_base) * AV_TIME_BASE);
                
                // 通过帧时长与帧数量计算每一帧的时间戳
                pkt->pts = (double)(video_frame_idx * frame_duration) / (double)(av_q2d(in_video_st->time_base) * AV_TIME_BASE);
                pkt->dts = pkt->dts;

                printf("video frame_duration :%jd, pkt.duration: %jd, pkt.pts: %jd\n", frame_duration, pkt->duration, pkt->pts);

                video_frame_idx++;
            }

            cur_video_pts = pkt->pts;
            pkt->stream_index = out_video_st_idx;
            output_stream = output_fmt_ctx->streams[out_video_st_idx];
        } else {

            // write audio
            input_stream = in_audio_st;
            result = av_read_frame(audio_fmt_ctx, pkt);
            if (result < 0) {
                printf("av_read_frame fail\n");
                av_packet_unref(pkt);
                break;
            }

            if (pkt->pts == AV_NOPTS_VALUE) {
                // 计算每一帧持续的时间
                int64_t frame_duration = (double)AV_TIME_BASE / av_q2d(in_audio_st->r_frame_rate);

                // 将帧时长的单位转换为以 time_base 为基准
                pkt->duration = (double)frame_duration / (double)(av_q2d(in_audio_st->time_base) * AV_TIME_BASE);
                
                // 通过帧时长与帧数量计算每一帧的时间戳
                pkt->pts = (double)(audio_frame_idx * frame_duration) / (double)(av_q2d(in_audio_st->time_base) * AV_TIME_BASE);
                pkt->dts = pkt->dts;

                printf("audio frame_duration :%jd, pkt.duration: %jd, pkt.pts: %jd\n", frame_duration, pkt->duration, pkt->pts);

                audio_frame_idx++;
            }

            cur_audio_pts = pkt->pts;
            pkt->stream_index = out_audio_st_idx;
            output_stream = output_fmt_ctx->streams[out_audio_st_idx];
        }

        // 从输入文件读取的码流包中保存的时间戳是以输入流的time_base为基准的，在写入输出文件之前需要转换为以输出流的time_base为基准
        pkt->pts = av_rescale_q_rnd(pkt->pts, input_stream->time_base, output_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, input_stream->time_base, output_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, input_stream->time_base, output_stream->time_base);
        
        printf("Final pts: %jd duration: %jd timebase: %d / %d\n", pkt->pts, pkt->duration, output_stream->time_base.num, output_stream->time_base.den);

        if (av_interleaved_write_frame(output_fmt_ctx, pkt) < 0) {
            printf("av_interleaved_write_frame fail\n");
            av_packet_unref(pkt);
            break;
        }

        av_packet_unref(pkt);
    }

    result = av_write_trailer(output_fmt_ctx);
    
    av_packet_free(&pkt);
    return result;
}

void destory_muxer()
{
    avformat_free_context(video_fmt_ctx);
    avformat_free_context(audio_fmt_ctx);

    if (!(output_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_fmt_ctx->pb);
    }

    avformat_free_context(output_fmt_ctx);
}
