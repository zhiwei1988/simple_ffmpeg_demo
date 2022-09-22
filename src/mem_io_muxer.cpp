// 以内存作为输入源进行 muxer

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/file.h>
#include <libavutil/common.h>
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <stdio.h>

static AVFormatContext* v_ifmt_ctx = nullptr; // 用于音频输入
static AVFormatContext* a_ifmt_ctx = nullptr; // 用于视频输入
static AVFormatContext* ofmt_ctx = nullptr; // 用于输出
static int32_t in_video_st_idx = -1;
static int32_t in_audio_st_idx = -1;
static int32_t out_video_st_idx = -1;
static int32_t out_audio_st_idx = -1;

static uint8_t *video_input_buffer = nullptr;
static size_t video_buffer_size;
static uint8_t *audio_input_buffer = nullptr;
static size_t audio_buffer_size;

static AVIOContext *audio_avio_ctx = nullptr;
static AVIOContext *video_avio_ctx = nullptr;
static uint8_t *audio_avio_ctx_buffer = nullptr;
static uint8_t *video_avio_ctx_buffer = nullptr;

static const size_t avio_ctx_buffer_size = 4096;

typedef struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
} buffer_data;

struct buffer_data v_bd = { 0 };
struct buffer_data a_bd = { 0 };

// 从内存中读取数据，不用的应用场景需要自己实现这个函数
static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    if (buf_size <= 0) {
       return -1; 
    }
        
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size; // 这里将设输入内存是一个连续的内存，每次读取一部分数据后，指针前移
    bd->size -= buf_size;

    return buf_size;
}

static int32_t open_input(char* filename, struct buffer_data *bd, uint8_t **input_buffer, size_t *buffer_size, 
                          AVIOContext **avio_ctx, uint8_t **avio_ctx_buffer, AVFormatContext** ifmt_ctx,
                          int32_t *st_idx, AVMediaType type)
{

     /* 将文件中的内容映射到内存 */
    int ret = av_file_map(filename, input_buffer, buffer_size, 0, nullptr);
    if (ret < 0) {
        return ret;
    }

    bd->ptr = *input_buffer;
    bd->size = *buffer_size;

    // 分配 io 缓存区
    *avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
    if (*avio_ctx_buffer == nullptr) {
        return -1;
    }

    // 分配 AVIOContext, 第三个参数 write_flag 为 0
    *avio_ctx = avio_alloc_context(*avio_ctx_buffer, avio_ctx_buffer_size,
                                 0, bd, &read_packet, nullptr, nullptr);
    if (*avio_ctx == nullptr) {
        return -1;
    }

    // 分配 AVFormatContext, 指定 AVFormatContext.pb 字段。必须在调用 avformat_open_input() 之前完成
    // 如果输入是文件 AVFormatContext 的分配可以交由 avformat_open_input 完成
    *ifmt_ctx = avformat_alloc_context();
    if (*ifmt_ctx == nullptr) {
        return -1;
    }

    (*ifmt_ctx)->pb = *avio_ctx;
    (*ifmt_ctx)->probesize = 1024; // 指定探测数据大小

    ret = avformat_open_input(ifmt_ctx, nullptr, nullptr, nullptr);
    if (ret < 0) {
        printf("Could not open input\n");
        return -1;
    }

    // 探测流信息
    ret = avformat_find_stream_info(*ifmt_ctx, nullptr);
    if (ret < 0) {
        printf("Could not find stream information\n");
        return -1;
    }

    // 查找复合条件的流索引
    *st_idx = av_find_best_stream(*ifmt_ctx, type, -1, -1, nullptr, 0);
    if (*st_idx < 0) {
        printf("find video stream in input video file failed\n");
        return -1;
    }

    return 0;
}

static int32_t open_output(char* output_file)
{
    int32_t result = 0;
    // 创建 AVFormatContext 结构的输出文件上下文句柄
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, output_file);
    if (result < 0) {
        printf("alloc output format context fail\n");
        return -1;
    }

    // 在创建输出文件句柄后，接下来要向其中添加媒体流
    // 添加媒体流可以使用函数 avformat_new_stream 实现
    const AVOutputFormat* fmt = ofmt_ctx->oformat;
    printf("Default video codec id: %d audio codec id: %d\n", fmt->video_codec, fmt->audio_codec);

    AVStream* video_stream = avformat_new_stream(ofmt_ctx, nullptr);
    if (video_stream == nullptr) {
        printf("add video stream to output format context fail\n");
        return -1;
    }

    // 新创建的 AVStream 结构基本是空的，缺少关键信息。为了将输入媒体流和输出媒体流的参数对齐，
    // 需要将输入文件中媒体流的参数（主要是码流编码参数）复制到输出文件对应的媒体流中。
    out_video_st_idx = video_stream->index;
    result = avcodec_parameters_copy(video_stream->codecpar, v_ifmt_ctx->streams[in_video_st_idx]->codecpar);
    if (result < 0) {
        printf("copy video codec paramaters failed!\n");
        return -1;
    }

    video_stream->id = ofmt_ctx->nb_streams - 1;
    video_stream->time_base = (AVRational){1, 90000};

    AVStream* audio_stream = avformat_new_stream(ofmt_ctx, nullptr);
    if (audio_stream == nullptr) {
        printf("add audio stream to output format context fail\n");
        return -1;
    }

    out_audio_st_idx = audio_stream->index;
    result = avcodec_parameters_copy(audio_stream->codecpar, a_ifmt_ctx->streams[in_audio_st_idx]->codecpar);
    if (result < 0) {
        printf("copy audio codec paramaters failed!\n");
        return -1;
    }

    audio_stream->id = ofmt_ctx->nb_streams - 1;
    audio_stream->time_base = (AVRational){1, audio_stream->codecpar->sample_rate};

    av_dump_format(ofmt_ctx, 0, output_file, 1);
    printf("output video idx: %d audio idx: %d\n", out_video_st_idx, out_audio_st_idx);

    // 有的输出格式不支持输出为文件
    if (!(fmt->flags & AVFMT_NOFILE)) {
        result = avio_open(&ofmt_ctx->pb, output_file, AVIO_FLAG_WRITE);
        if (result < 0) {
            printf("avio_open output file fail\n");
            return -1;
        }
    }

    return result;
}

static int32_t do_muxing()
{
    int32_t result = 0;
    int64_t pre_video_dts = -1;
    int64_t cur_video_pts = 0;
    int64_t cur_audio_pts = 0;

    AVStream* in_video_st = v_ifmt_ctx->streams[in_video_st_idx];
    AVStream* in_audio_st = a_ifmt_ctx->streams[in_audio_st_idx];
    AVStream* output_stream = nullptr;
    AVStream* input_stream = nullptr;

    int32_t video_frame_idx = 0;
    int32_t audio_frame_idx = 0;
    result = avformat_write_header(ofmt_ctx, nullptr);
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
            result = av_read_frame(v_ifmt_ctx, pkt);
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
                
                // 计算每一帧持续的时间, 以 AV_TIME_BASE 为时间基准
                int64_t frame_duration = (double)AV_TIME_BASE / av_q2d(in_video_st->r_frame_rate);

                // 将帧时长的单位转换为以 time_base 为基准
                pkt->duration = (double)frame_duration / (double)(av_q2d(in_video_st->time_base) * AV_TIME_BASE);
                
                // 通过帧时长与帧数量计算每一帧的时间戳, 以 time_base 为基准
                pkt->pts = (double)(video_frame_idx * frame_duration) / (double)(av_q2d(in_video_st->time_base) * AV_TIME_BASE);
                pkt->dts = pkt->dts;

                printf("video frame_duration :%jd, pkt.duration: %jd, pkt.pts: %jd\n", frame_duration, pkt->duration, pkt->pts);

                video_frame_idx++;
            }

            cur_video_pts = pkt->pts;
            pkt->stream_index = out_video_st_idx;
            output_stream = ofmt_ctx->streams[out_video_st_idx];
        } else {

            // write audio
            input_stream = in_audio_st;
            result = av_read_frame(a_ifmt_ctx, pkt);
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
            output_stream = ofmt_ctx->streams[out_audio_st_idx];
        }

        // 从输入文件读取的码流包中保存的时间戳是以输入流的time_base为基准的，在写入输出文件之前需要转换为以输出流的time_base为基准
        pkt->pts = av_rescale_q_rnd(pkt->pts, input_stream->time_base, output_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, input_stream->time_base, output_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, input_stream->time_base, output_stream->time_base);
        
        printf("Final pts: %jd duration: %jd timebase: %d / %d\n", pkt->pts, pkt->duration, output_stream->time_base.num, output_stream->time_base.den);

        if (av_interleaved_write_frame(ofmt_ctx, pkt) < 0) {
            printf("av_interleaved_write_frame fail\n");
            av_packet_unref(pkt);
            break;
        }

        av_packet_unref(pkt);
    }

    result = av_write_trailer(ofmt_ctx);
    
    av_packet_free(&pkt);
    return result;
}

int main(int argc, char *argv[])
{

    int ret = 0;
    if (argc != 3) {
//        printf("usage: %s video_input_file audio_input_file\n" argv[0]);
        return 1;
    }
    
    char* video_input_filename = argv[1];
    char* audio_input_filename = argv[2];

    ret = open_input(video_input_filename, &v_bd, &video_input_buffer, &video_buffer_size,
                     &video_avio_ctx, &video_avio_ctx_buffer, &v_ifmt_ctx, &in_video_st_idx, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
        goto end;
    }

    ret = open_input(audio_input_filename, &a_bd, &audio_input_buffer, &audio_buffer_size,
                     &audio_avio_ctx, &audio_avio_ctx_buffer, &a_ifmt_ctx, &in_audio_st_idx, AVMEDIA_TYPE_AUDIO);
    if (ret < 0) {
        goto end;
    }

    ret = open_output("test.mp4");
    if (ret < 0) {
        goto end;
    }

    do_muxing();

end:
    avformat_free_context(v_ifmt_ctx);
    avformat_free_context(a_ifmt_ctx);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }

    avformat_free_context(ofmt_ctx);


    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (audio_avio_ctx) {
        av_freep(&audio_avio_ctx->buffer);
        av_freep(&audio_avio_ctx);
    }

    if (video_avio_ctx) {
        av_freep(&video_avio_ctx->buffer);
        av_freep(&video_avio_ctx);
    }

    av_file_unmap(video_input_buffer, video_buffer_size);
    av_file_unmap(audio_input_buffer, audio_buffer_size);

    return 0;
}