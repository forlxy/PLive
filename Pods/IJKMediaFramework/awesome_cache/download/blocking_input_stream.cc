//
// Created by 帅龙成 on 2018/7/8.
//

#include "blocking_input_stream.h"
#include "cache_errors.h"
#include "ac_log.h"


namespace kuaishou {
namespace cache {

BlockingInputStream::BlockingInputStream(uint32_t capacity) :
    eof_signaled_(false),
    close_signaled_(false),
    error_code_(0),
    capacity_(capacity) {
    ring_buffer_ = std::unique_ptr<BlockingRingBuffer>(new BlockingRingBuffer(capacity));
}

int BlockingInputStream::Read(uint8_t* buf, int32_t offset, int32_t len) {
    int ret = ring_buffer_->PopBuf(buf, offset, len);

    if (ret > 0) {
        return ret;
    } else if (error_code_ != 0) {
        return error_code_;
    } else if (ret == 0) {
        return kResultBlockingInputStreamReadReturnZero;
    } else {
        LOG_ERROR_DETAIL("BlockingInputStream::Read, PopBuf, ret:%d \n", ret);
        switch (ret) {
            case kBlockingRingBufferInitFail:
                error_code_ = kResultBlockingInputStreamReadInitFail;
                break;
            case kBlockingRingBufferInterrupted:
                if (eof_signaled_) {
                    error_code_ = kResultBlockingInputStreamEndOfStram;
                } else {
                    error_code_ = kResultBlockingInputStreamReadInterrupted;
                }
                break;
            case kBlockingRingBufferInvalidArgs:
                error_code_ = kResultBlockingInputStreamReadInvalidArgs;
                break;
            case kBlockingRingBufferInnerError:
            default:
                error_code_ = kResultBlockingInputStreamReadInnerError;
                break;
        }
        return error_code_;
    }
}

bool BlockingInputStream::HasMoreData() {
    bool no_more_data = eof_signaled_ && ring_buffer_->IsEmpty();
    return !no_more_data;
}

int BlockingInputStream::error_code() {
    return error_code_;
}

int BlockingInputStream::FeedDataSync(uint8_t* data, int32_t len, int32_t& used_bytes) {
    if (error_code_ != 0) {
        return error_code_;
    }

    int ret = ring_buffer_->PushBuf(data, len, used_bytes);
    if (ret < 0) {
        LOG_ERROR_DETAIL("BlockingInputStream::FeedDataSync, PushBuf, ret:%d \n", ret);
        error_code_ = kResultExceptionWriteFailed;
        return error_code_;
    }
    return ret;
}

void BlockingInputStream::Close() {
    close_signaled_ = true;
    if (ring_buffer_) {
        ring_buffer_->Interrupt();
    }
}

//void BlockingInputStream::Reset() {
// has no implementation ,Reset is not good synatic
//  if (ring_buffer_) {
//    ring_buffer_->Interrupt();
//  }
//  ring_buffer_ = std::unique_ptr<BlockingRingBuffer>(new BlockingRingBuffer(capacity_));
//  error_code_ = 0;
//  eof_signaled_ = false;
//  close_signaled_ = false;
//}

void BlockingInputStream::EndOfStream(int error_code) {
    error_code_ = error_code;
    eof_signaled_ = true;
    if (ring_buffer_) {
        ring_buffer_->Interrupt();
    }
}

}
}
