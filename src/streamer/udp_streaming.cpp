/**
* 使用 rtp over udp 对 h264 文件进行串流
*/


#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#ifdef __cplusplus
};
#endif

int main(int argc, char* argv[])
{    
    const char* in_filename = "outdoor.h264";
    const char* out_filename = "udp://192.168.200.1:1234";

    // 打开输入
    AVFormatContext *ifmt_ctx = nullptr;
    int32_t ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0);
    if (ret < 0) {
        printf( "Could not open input file.\n");
        goto end;
    }

    // 查找输入流信息
    ret = avformat_find_stream_info(ifmt_ctx, 0);
    if (ret < 0) {
        printf( "Failed to retrieve input stream information\n");
        goto end;
    }

    int32_t video_index = -1;
    for(int32_t i = 0; i < ifmt_ctx->nb_streams; i++) {
        if(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
            break;
        }
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    // 输出
    AVFormatContext *ofmt_ctx = nullptr;
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, "rtp", out_filename);
    if (ofmt_ctx == nullptr) {
        printf( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    AVOutputFormat *ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        // 根据输入流创建输出流，这里我们只处理视频流
        if (i != video_index) {
            continue;
        }

        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        if (out_stream == nullptr) {
            printf( "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        // 复制 AVCodecContext 的设置
        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            printf( "Failed to copy context from input to output stream codec context\n");
            goto end;
        }

#if 0
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
#endif
    }

    // Dump Format------------------
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    // 打开输出 URL
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf( "Could not open output URL '%s'\n", out_filename);
            goto end;
        }
    }

    // 写文件头
    ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0) {
        printf( "Error occurred when opening output URL\n");
        goto end;
    }

    int64_t start_time = av_gettime();
    int32_t frame_index=0;
    while (1) {
        
        AVStream *out_stream = nullptr;

        // 获取一个AVPacket
        AVPacket pkt;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            break;
        }

        if (pkt.stream_index != video_index) {
            continue;
        }
            
        // FIX：No PTS (Example: Raw H.264)
        // Simple Write PTS
        if(pkt.pts == AV_NOPTS_VALUE){
            //Write PTS
            AVRational time_base = ifmt_ctx->streams[video_index]->time_base;
            
            // Duration between 2 frames (us)
            // 根据帧计算两帧之间的间隔
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[video_index]->r_frame_rate);

            //Parameters
            // 以下计算值都是以输入流的 time_base 为基准
            pkt.pts = (double)(frame_index * calc_duration) / (double)(av_q2d(time_base) * AV_TIME_BASE);
            pkt.dts = pkt.pts;
            pkt.duration = (double)calc_duration / (double)(av_q2d(time_base) * AV_TIME_BASE);
        }

        //Important:Delay 保证按帧率进行发送
        AVRational time_base = ifmt_ctx->streams[video_index]->time_base;
        AVRational time_base_q = {1, AV_TIME_BASE};
        int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
        int64_t now_time = av_gettime() - start_time;
        if (pts_time > now_time) {
            av_usleep(pts_time - now_time);
        }   

        AVStream *in_stream  = ifmt_ctx->streams[pkt.stream_index];
        AVStream *out_stream = ofmt_ctx->streams[pkt.stream_index];

        // 转换PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        printf("Send %8d video frames to output URL\n",frame_index);
        frame_index++;
        
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            printf( "Error muxing packet\n");
            break;
        }
        
        av_free_packet(&pkt);
    }

    // 写文件尾
    av_write_trailer(ofmt_ctx);

end:
    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) {
        avio_close(ofmt_ctx->pb);
    }

    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf( "Error occurred.\n");
        return -1;
    }
    
    return 0;
}