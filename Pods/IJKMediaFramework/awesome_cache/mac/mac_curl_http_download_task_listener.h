//
// Created by MarshallShuai on 2019/7/4.
//

#pragma once

#include "v2/cache/cache_def_v2.h"

class MacScopeCurlHttpTaskListener : public kuaishou::cache::ScopeTaskListener {
  public:
    MacScopeCurlHttpTaskListener(int alloc_buf_len = kMaxScopeBytes) :
        received_bytes_(0), last_error_(0), position_(0), alloc_buf_len_(alloc_buf_len) {
        buf_ = new uint8_t[alloc_buf_len];
    }

    ~MacScopeCurlHttpTaskListener() {
        delete[]buf_;
    }


    virtual void OnConnectionInfoParsed(const kuaishou::cache::ConnectionInfoV2& info) {
    }

    virtual void OnReceiveData(uint8_t* data_buf, int64_t data_len) {
        assert(data_len + received_bytes_ <= alloc_buf_len_);
        memcpy(buf_ + received_bytes_, data_buf, data_len);
        received_bytes_ += data_len;
    }

    virtual void OnDownloadComplete(int32_t error, int32_t stop_reason) {
        last_error_ = error;
        stop_reason_ = stop_reason;
    }

    /**
     * 表示这段buf在整个文件流中的position
     * @param position
     */
    void SetPosition(int64_t position) {
        position_ = position;
    }

    int64_t alloc_buf_len_;

    int64_t position_;
    uint8_t* buf_;
    int64_t received_bytes_;

    int32_t last_error_;
    int32_t stop_reason_;
};
