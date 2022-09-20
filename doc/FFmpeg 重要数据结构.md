# FFmpeg 重要数据结构

{{TOC}}

## 结构体之间的联系

![](https://my-image-asset.s3.ap-northeast-1.amazonaws.com/uPic/2022-03-06-16-47.jpg)

## AVFormatContext 

在使用 FFmpeg 进行开发的时候，AVFormatContext 是一个贯穿始终的数据结构，很多函数都要用到它作为参数。它是 FFmpeg 解封装（flv，mp4，rmvb，avi）功能的结构体。

- `AVInputFormat *iformat`：输入容器格式数据
- `AVOutputFormat *oformat`：输出容器格式数据
- `AVIOContext *pb`：数据的缓存
- `unsigned int nb_streams`：视音频流的个数
- `AVStream **streams`：视音频流
- `char filename[1024]`：文件名
- `char *url`：输入/输出URL
- `int64_t duration`：时长（单位：微秒us，转换为秒需要除以1000000）
- `int bit_rate`：比特率（单位bps，转换为kbps需要除以1000）
- `int packet_size`：packet的长度
- `AVDictionary *metadata`：元数据

## AVIOContext 

AVIOContext 是 FFmpeg 管理输入输出数据的结构体。

- `unsigned char *buffer`：缓存开始位置
- `int buffer_size`：缓存大小（默认32768）
- `unsigned char *buf_ptr`：当前指针读取到的位置
- `unsigned char *buf_end`：缓存结束的位置
- `*void opaque`：URLContext结构体


## AVStream

AVStream 是存储单个视频/音频流信息的结构体。

- `int index`：标识该视频/音频流(AVFormatContext中)
- `AVCodecContext *codec`：指向该视频/音频流的 AVCodecContext（它们是一一对应的关系）
- `AVRational time_base`：时基。通过该值可以把PTS，DTS转化为真正的时间（Unix 时间戳）
- `int64_t duration`：该视频/音频流长度
- `int64_t nb_frames`：该流中已知的帧数
- `AVDictionary *metadata`：元数据信息
- `AVRational avg_frame_rate`：帧率
- `AVPacket attached_pic`：附带的图片。比如说一些MP3，AAC音频文件附带的专辑封面

## AVCodecContext

- `enum AVMediaType codec_type`：编解码器的类型（视频，音频，字幕…）
- `struct AVCodec *codec`：采用的解码器AVCodec（H.264, MPEG2…）
- `int bit_rate`：平均比特率
- `uint8_t *extradata; int extradata_size`：针对特定编码器包含的附加信息（例如对于H.264解码器来说，存储SPS，PPS等）
- `AVRational time_base`：根据该参数，可以把PTS转化为实际的时间（单位为秒s）
- `int width, height`：如果是视频的话，代表宽和高
- `int refs`：运动估计参考帧的个数（H.264 的话会有多帧，MPEG2 这类的一般就没有了）
- `int sample_rate`：采样率（音频）
- `int channels`：声道数（音频）
- `enum AVSampleFormat sample_fmt`：采样格式
- `int frame_size`：帧中每个通道的采样率（音频）
- `int profile`：型（H.264 里面就有，其他编码标准应该也有）
- `int level`：级（和 profile 差不太多）

## AVCodec

AVCodec 是存储编解码器信息的结构体。

- `const char *name`：编解码器的名字，比较短
- `const char *long_name`：编解码器的名字，全称，比较长
- `enum AVMediaType type`：指明了类型，是视频，音频，还是字幕
- `enum AVCodecID id`：ID，不重复
- `const AVRational *supported_framerates`：支持的帧率（仅视频）
- `const enum AVPixelFormat *pix_fmts`：支持的像素格式（仅视频）
- `const int *supported_samplerates`：支持的采样率（仅音频）
- `const enum AVSampleFormat *sample_fmts`：支持的采样格式（仅音频）
- `const uint64_t *channel_layouts`：支持的声道数（仅音频）
- `int priv_data_size`：私有数据的大小

## AVPacket 

AVPacket 是存储压缩编码数据相关信息的结构体。例如对于 H.264 来说。1 个 AVPacket 的 data 通常对应一个 NAL。注意：在这里只是对应，而不是一模一样，他们之间有微小的差别。

- `int64_t pts`：显示时间戳
- `int64_t dts`：解码时间戳
- `uint8_t *data`：压缩编码的数据
- `int size`：data 的大小
- `int stream_index`：标识该AVPacket所属的视频/音频流

## AVFrame

AVFrame 结构体一般用于存储原始数据（即非压缩数据，例如对视频来说是YUV，RGB，对音频来说是 PCM），此外还包含了一些相关的信息。比如说，解码的时候存储了宏块类型表，QP表，运动矢量表等数据。编码的时候也存储了相关的数据。

- `uint8_t *data[AV_NUM_DATA_POINTERS]`：解码后原始数据（对视频来说是YUV、RGB，对音频来说是PCM）
- `int linesize[AV_NUM_DATA_POINTERS]`：data 中“一行”数据的大小。注意：未必等于图像的宽，一般大于图像的宽。
- `int width, height`：视频帧宽和高（1920x1080, 1280x720…）
- `int nb_samples`：音频的一个 AVFrame 中可能包含多个音频帧，在此标记包含了几个
- `int format`：解码后原始数据类型（YUV420，YUV422，RGB24…），未知或未设置为-1。
- `int key_frame`：是否是关键帧
- `enum AVPictureType pict_type`：帧类型（I,B,P…）
- `AVRational sample_aspect_ratio`：宽高比（16:9，4:3…）
- `int64_t pts`：显示时间戳
- `int coded_picture_number`：编码帧序号
- `int display_picture_number`：显示帧序号
- `int8_t *qscale_table`：QP表
- `int channels`：音频通道数
- `int interlaced_frame`：是否是隔行扫描