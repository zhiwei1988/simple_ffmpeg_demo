FFmpeg 关键结构体

## AVStream

AVStream 是存储每一个视频/音频流信息的结构体。结构体的定义位于 avformat.h 文件中。

```c++
/**
 * Rational number (pair of numerator and denominator).
 */
typedef struct AVRational{
    int num; ///< 分子
    int den; ///< 分母
} AVRational;
```

| 变量名 | 类型 | 描述 |
|:--|:--|:--|
| index |int | stream index in AVFormatContext |
| time_base |AVRational | 以 s 为基准的时间单位，可以通过 pts * time_base，得到以 s 为单位的时间戳 |
| duration |int64_t | 解码时：流的持续时间，以 time_base 为基准。如果一个源文件没有指定持续时间，但指定了比特率，这个值将从比特率和文件大小中计算出来|
| metadata |AVDictionary * | 元数据信息 |
| avg_frame_rate |AVRational | 平均帧率 |

## AVPacket

用于存储编码压缩后的音视频数据。对于视频，它通常应包含一个压缩帧。对于音频来说，它可能包含几个压缩的帧。

| 变量名 | 类型 | 描述 |
|:--|:--|:--|
| data | uint8_t * | 压缩后的数据 |
| size | int | data 的大小 |
| stream_index | int | 标识该 AVPacket 所属的视频/音频流 |

