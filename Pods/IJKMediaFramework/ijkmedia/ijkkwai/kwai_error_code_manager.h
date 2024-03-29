//
// Created by 帅龙成 on 2018/5/29.
//
#pragma once

#include "cache_errors.h"


// AwesomeCache的错误码定义为 (-4000, -1000), 播放器转移的错误码从 -5000开始
#define EIJK_FFMPEG_ERROR_BASE                  -5000
#define EIJK_KWAI_INNER_ERROR_BASE              -5100

// 1.这个值用来给位置的error做偏移，为了让后台能统计到错误而设计的。同时为了ffmpeg的错误码不会和上面的 -1~-15重叠，
// 我们让 EIJK_UNKNOWN_ERROR等于一个较大的地址（比如-1000）来作为基地址
// 2.这个值同时又和业务层的重连逻辑耦合起来了，上层本来是通过判断一系列固定的错误值来判断是否isCriticalErrorInMediaPlayer，
// 但是现在有透传错误码统计的需求，所以这个值并不能列举全面，只能通过判断某个范围来知道是 UNKNOWN_ERROR,
// 所以这个范围的选择就是一个耦合的结果，ijkplayer_android_def.h里的MEDIA_ERROR_xxx都是 -10001之类的值，所以这里为了
// 让上层能判断出未知错误的范围值，这里选择 -20000
#define EIJK_UNKNOWN_ERROR_BASE         (-20000)    //未知错误

#define KWAI_PLAYER_ERROR_CODES(X) \
    X(-1 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_PROTOCOL_NOT_FOUND, "EIJK_AVERROR_PROTOCOL_NOT_FOUND", "不支持的protocol") \
    X(-2 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_EIO, "EIJK_AVERROR_EIO", "读写数据异常") \
    X(-3 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_EMFILE, "EIJK_AVERROR_EMFILE", "创建socket失败") \
    X(-4 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_ETIMEDOUT, "EIJK_AVERROR_ETIMEDOUT", "链接服务器失败") \
    X(-5 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_HTTP_BAD_REQUEST, "EIJK_AVERROR_HTTP_BAD_REQUEST", "http 400") \
    X(-6 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_HTTP_UNAUTHORIZED, "EIJK_AVERROR_HTTP_UNAUTHORIZED", "401") \
    X(-7 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_HTTP_FORBIDDEN, "EIJK_AVERROR_HTTP_FORBIDDEN", "403") \
    X(-8 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_HTTP_NOT_FOUND, "EIJK_AVERROR_HTTP_NOT_FOUND", "404") \
    X(-9 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_HTTP_OTHER_4XX, "EIJK_AVERROR_HTTP_OTHER_4XX", "4xx") \
    X(-10 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_HTTP_SERVER_ERROR, "EIJK_AVERROR_HTTP_SERVER_ERROR", "5xx") \
    X(-11 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_INVALIDDATA, "EIJK_AVERROR_INVALIDDATA", "无效的数据类型") \
    X(-12 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_EXIT, "EIJK_AVERROR_EXIT", "大部分情况代表着异常的退出") \
    X(-13 + EIJK_FFMPEG_ERROR_BASE, EIJK_AVERROR_EOF, "EIJK_AVERROR_EOF", "End Of File") \
    \
    X(-1 + EIJK_KWAI_INNER_ERROR_BASE, EIJK_KWAI_READ_DATA_IO_TIMEOUT, "EIJK_KWAI_READ_DATA_IO_TIMEOUT", "超时") \
    X(-2 + EIJK_KWAI_INNER_ERROR_BASE, EIJK_KWAI_UNSUPPORT_VCODEC, "EIJK_KWAI_UNSUPPORT_VCODEC", "不支持的codec") \
    X(-3 + EIJK_KWAI_INNER_ERROR_BASE, EIJK_KWAI_UNSUPPORT_ACODEC, "EIJK_KWAI_UNSUPPORT_ACODEC", "不支持的codec") \
    X(-4 + EIJK_KWAI_INNER_ERROR_BASE, EIJK_KWAI_NO_MEMORY, "EIJK_KWAI_NO_MEMORY", "内存不足") \
    X(-5 + EIJK_KWAI_INNER_ERROR_BASE, EIJK_KWAI_DEC_ERR, "EIJK_KWAI_DEC_ERR", "解码报错") \
    X(-6 + EIJK_KWAI_INNER_ERROR_BASE, EIJK_KWAI_BLOCK_ERR, "EIJK_KWAI_BLOCK_ERR", "卡顿报错For A1")


#define KWAI_PLAYER_ERROR_ENUM(ID, NAME, ERR_MSG, ERR_DESC) NAME = ID,
enum {
    KWAI_PLAYER_ERROR_CODES(KWAI_PLAYER_ERROR_ENUM)
};

const char* kwai_error_code_to_string(int error);
