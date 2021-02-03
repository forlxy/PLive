//
// Created by MarshallShuai on 2018/11/5.
//

#include "kwai_error_code_manager.h"
#include <libavutil/common.h>
#include "ff_fferror.h"
#include "ijksdl/ijksdl_log.h"

int convert_to_kwai_error_code(int error) {
    switch (error) {
        case AVERROR_PROTOCOL_NOT_FOUND:
            return EIJK_AVERROR_PROTOCOL_NOT_FOUND;
        case AVERROR(EIO):
        case AVERROR_EIO:
            return EIJK_AVERROR_EIO;
        case AVERROR(EMFILE):
            return EIJK_AVERROR_EMFILE;
        case AVERROR(ETIMEDOUT):
            return EIJK_AVERROR_ETIMEDOUT;
        case AVERROR_HTTP_BAD_REQUEST:
            return EIJK_AVERROR_HTTP_BAD_REQUEST;
        case AVERROR_HTTP_UNAUTHORIZED:
            return EIJK_AVERROR_HTTP_UNAUTHORIZED;
        case AVERROR_HTTP_FORBIDDEN:
            return EIJK_AVERROR_HTTP_FORBIDDEN;
        case AVERROR_HTTP_NOT_FOUND:
            return EIJK_AVERROR_HTTP_NOT_FOUND;
        case AVERROR_HTTP_OTHER_4XX:
            return EIJK_AVERROR_HTTP_OTHER_4XX;
        case AVERROR_HTTP_SERVER_ERROR:
            return EIJK_AVERROR_HTTP_SERVER_ERROR;
        case AVERROR_INVALIDDATA:
            return EIJK_AVERROR_INVALIDDATA;
        case AVERROR_EXIT:
            return EIJK_AVERROR_EXIT;
        case AVERROR_EOF:
            return EIJK_AVERROR_EOF;
        default:
            //unrecognized ffmpeg error code
            return error + EIJK_UNKNOWN_ERROR_BASE;  // 这样比直接返回EIJK_UNKNOWN_ERROR更好，排查问题的时候，夹带了error信息
    }
}

