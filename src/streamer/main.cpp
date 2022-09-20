//
// Created by ZhiWei Tan on 2/9/22.
//

#include <iostream>
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"


int main()
{
    std::cout << "hello world" << std::endl;
    return 0;
}

#define LOGD(format, ...) av_log(nullptr, AV_LOG_DEBUG, format, ##__VA_ARGS__)

int ff_dump_stream_info(char* url)
{
    AVFormatContext *ic = avformat_alloc_context();
    if (avformat_open_input(&ic, url, nullptr, nullptr) < 0) {
        LOGD("could not open source: %s", url);
        return -1;
    }

    if (avformat_find_stream_info(ic, nullptr) < 0) {
        LOGD("could not find stream information");
        avformat_close_input(&ic);
        return -1;
    }

    LOGD(" -------- dumping stream info -------- ");

    LOGD("input format: %s", ic->iformat->name);
    LOGD("nb_streams: %u", ic->nb_streams);

    auto start_time = ic->start_time / AV_TIME_BASE; // 第一帧的时间戳
    LOGD("start_time: %ld", start_time);

    auto duration = ic->duration / AV_TIME_BASE;
    LOGD("duration: %ld", duration);

    auto video_stream_idx = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_idx >= 0) {
        AVStream* video_stream = ic->streams[video_stream_idx];
        AVCodec* codec;
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (codecContext == nullptr){
            LOGD("could not alloc avcodec context");
            avformat_close_input(&ic);
            return -1;
        }

        auto result = avcodec_parameters_to_context(codecContext, ic->streams[video_stream_idx]->codecpar);
        if (result < 0) {
            LOGD("set param to avcodec context fail");
            avformat_close_input(&ic);
            avcodec_free_context(&codecContext);
            return -1;
        }

        LOGD("video nb_frames: %jd", video_stream->nb_frames);
        LOGD("video codec_id: %u", codecContext->codec_id);
        LOGD("video codec_name: %s", avcodec_get_name(codecContext->codec_id));
        LOGD("video width x height: %d x %d", video_stream->codecpar->width, video_stream->codecpar->height);
        LOGD("video pix_fmt: %d", codecContext->pix_fmt);
        LOGD("video bitrate %ld kb/s", video_stream->codecpar->bit_rate / 1024);
        LOGD("video avg_frame_rate: %d fps", video_stream->avg_frame_rate.num / video_stream->avg_frame_rate.den);
    }
}

