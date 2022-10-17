/*
 * AVIOContext 主要使用逻辑: 我们有一块大的视频文件 buffer, 然后 ffmpeg 对这块数据的访问借助于 AVIOContext, 
 * AVIOContext 自己维护了一个小的 buffer(注意:这个小的buffer也是需要我们手动给分配), 之后 ffmpeg 内部解封转、解码
 * 需要的数据都从 AVIOContext 这个小 buffer 获取, AVIOContext 上这个小 buffer 数据不足时又自己从视频大 buffer 中不断补充数据
 */

 /**
 * Bytestream IO Context.
 * New fields can be added to the end with minor version bumps.
 * Removal, reordering and changes to existing fields require a major
 * version bump.
 * sizeof(AVIOContext) must not be used outside libav*.
 *
 * @note None of the function pointers in AVIOContext should be called
 *       directly, they should only be set by the client application
 *       when implementing custom I/O. Normally these are set to the
 *       function pointers specified in avio_alloc_context()
 */
typedef struct AVIOContext {
    /**
     * A class for private options.
     *
     * If this AVIOContext is created by avio_open2(), av_class is set and
     * passes the options down to protocols.
     *
     * If this AVIOContext is manually allocated, then av_class may be set by
     * the caller.
     *
     * warning -- this field can be NULL, be sure to not pass this AVIOContext
     * to any av_opt_* functions in that case.
     */
    const AVClass *av_class;

    /*
     * The following shows the relationship between buffer, buf_ptr,
     * buf_ptr_max, buf_end, buf_size, and pos, when reading and when writing
     * (since AVIOContext is used for both):
     *
     **********************************************************************************
     *                                   READING
     **********************************************************************************
     *
     *                            |              buffer_size              |
     *                            |---------------------------------------|
     *                            |                                       |
     *
     *                         buffer          buf_ptr       buf_end
     *                            +---------------+-----------------------+
     *                            |/ / / / / / / /|/ / / / / / /|         |
     *  read buffer:              |/ / consumed / | to be read /|         |
     *                            |/ / / / / / / /|/ / / / / / /|         |
     *                            +---------------+-----------------------+
     *
     *                                                         pos
     *              +-------------------------------------------+-----------------+
     *  input file: |                                           |                 |
     *              +-------------------------------------------+-----------------+
     *
     *
     **********************************************************************************
     *                                   WRITING
     **********************************************************************************
     *
     *                             |          buffer_size                 |
     *                             |--------------------------------------|
     *                             |                                      |
     *
     *                                                buf_ptr_max
     *                          buffer                 (buf_ptr)       buf_end
     *                             +-----------------------+--------------+
     *                             |/ / / / / / / / / / / /|              |
     *  write buffer:              | / / to be flushed / / |              |
     *                             |/ / / / / / / / / / / /|              |
     *                             +-----------------------+--------------+
     *                               buf_ptr can be in this
     *                               due to a backward seek
     *
     *                            pos
     *               +-------------+----------------------------------------------+
     *  output file: |             |                                              |
     *               +-------------+----------------------------------------------+
     *
     */
    unsigned char *buffer;  // 即 AVIOContext 那个小buffer, 通过 avio_alloc_context() 来分配 AVIOContext 时作为参数传递
    int buffer_size;        // 小 buffer 的大小
    unsigned char *buf_ptr; // 小 buffer 中的数据当前被消耗的位置
    unsigned char *buf_end; /**< End of the data, may be less than
                                 buffer+buffer_size if the read function returned
                                 less data than requested, e.g. for streams where
                                 no more data has been received yet. */
                            // 小 buffer 中数据的结束位置
    void *opaque;           /**< A private pointer, passed to the read/write/seek/...
                                 functions. */
                            // 是一个自定义的结构体, 存储视频大 buffer 的信息, 如 buffer 开始位置, 视频 buffer 长度, 这个结构会回传给 read_packet、write_packet、seek 回调函数
    int (*read_packet)(void *opaque, uint8_t *buf, int buf_size); // 需要自己实现的一个用视频大 buffer 数据填充小 buffer 的回调函数
    int (*write_packet)(void *opaque, uint8_t *buf, int buf_size); // 自己实现把 AVIOContext 中的小 buffer 数据写到某处的回调函数
    int64_t (*seek)(void *opaque, int64_t offset, int whence); // // 也是自己实现的用来在视频大 buffer 中 seek 的函数
    int64_t pos;            /**< position in the file of the current buffer */
    int eof_reached;        /**< true if was unable to read due to error or eof */
    int write_flag;         /**< true if open for writing */
    int max_packet_size;
    unsigned long checksum;
    unsigned char *checksum_ptr;
    unsigned long (*update_checksum)(unsigned long checksum, const uint8_t *buf, unsigned int size);
    int error;              /**< contains the error code or 0 if no error happened */
    /**
     * Pause or resume playback for network streaming protocols - e.g. MMS.
     */
    int (*read_pause)(void *opaque, int pause);
    /**
     * Seek to a given timestamp in stream with the specified stream_index.
     * Needed for some network streaming protocols which don't support seeking
     * to byte position.
     */
    int64_t (*read_seek)(void *opaque, int stream_index,
                         int64_t timestamp, int flags);
    /**
     * A combination of AVIO_SEEKABLE_ flags or 0 when the stream is not seekable.
     */
    int seekable;

    /**
     * max filesize, used to limit allocations
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int64_t maxsize;

    /**
     * avio_read and avio_write should if possible be satisfied directly
     * instead of going through a buffer, and avio_seek will always
     * call the underlying seek function directly.
     */
    int direct;

    /**
     * Bytes read statistic
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int64_t bytes_read;

    /**
     * seek statistic
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int seek_count;

    /**
     * writeout statistic
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int writeout_count;

    /**
     * Original buffer size
     * used internally after probing and ensure seekback to reset the buffer size
     * This field is internal to libavformat and access from outside is not allowed.
     */
    int orig_buffer_size;

    /**
     * Threshold to favor readahead over seek.
     * This is current internal only, do not use from outside.
     */
    int short_seek_threshold;

    /**
     * ',' separated list of allowed protocols.
     */
    const char *protocol_whitelist;

    /**
     * ',' separated list of disallowed protocols.
     */
    const char *protocol_blacklist;

    /**
     * A callback that is used instead of write_packet.
     */
    int (*write_data_type)(void *opaque, uint8_t *buf, int buf_size,
                           enum AVIODataMarkerType type, int64_t time);
    /**
     * If set, don't call write_data_type separately for AVIO_DATA_MARKER_BOUNDARY_POINT,
     * but ignore them and treat them as AVIO_DATA_MARKER_UNKNOWN (to avoid needlessly
     * small chunks of data returned from the callback).
     */
    int ignore_boundary_point;

    /**
     * Internal, not meant to be used from outside of AVIOContext.
     */
    enum AVIODataMarkerType current_type;
    int64_t last_time;

    /**
     * A callback that is used instead of short_seek_threshold.
     * This is current internal only, do not use from outside.
     */
    int (*short_seek_get)(void *opaque);

    int64_t written;

    /**
     * Maximum reached position before a backward seek in the write buffer,
     * used keeping track of already written data for a later flush.
     */
    unsigned char *buf_ptr_max;

    /**
     * Try to buffer at least this amount of data before flushing it
     */
    int min_packet_size;
} AVIOContext;