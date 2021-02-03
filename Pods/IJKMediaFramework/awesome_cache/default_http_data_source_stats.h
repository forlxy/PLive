//
//  default_http_data_source_stats.hpp
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/8/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#pragma once

#include "stats/json_stats.h"
#include "download/connection_info.h"
#include "include/data_spec.h"
#include <stdint.h>

namespace kuaishou {
namespace cache {

class DefaultHttpDataSourceStats : public JsonStats {

  public :
    DefaultHttpDataSourceStats(const std::string& type):
        JsonStats(type),
        read_len_(0),
        error_(0) {

    }

    void  SaveDataSpec(const DataSpec& spec) {
        RETURN_IF_DISABLE
        uri_ = spec.uri;
        pos_ = spec.position;
    }

    void  OnMakeConnectionFinish(const ConnectionInfo& info) {
        RETURN_IF_DISABLE
        host_ = info.host;
        ip_ = info.ip;
        response_code_ = info.response_code;
        content_length_ = info.content_length;
        kwai_sign_ = info.sign;
    }

    void  OnError(int error) {
        RETURN_IF_DISABLE
        error_ = error;
    }

    void OnReadBytes(int bytes) {
        RETURN_IF_DISABLE
        read_len_ += bytes;
    }

  protected:
    virtual void FillJson() {
        RETURN_IF_DISABLE
        stats_["aType"] = type_;
        stats_["uri"] = uri_;
        stats_["host"] = host_;
        stats_["ip"] = ip_;
        stats_["kwai_sign"] = kwai_sign_;
        stats_["resp_code"] = response_code_;
        stats_["pos"] = pos_;
        stats_["content_len"] = content_length_;
        stats_["error"] = error_;
        stats_["read_len"] = read_len_;

    };

  private:
    // from DataSpec(config)
    std::string uri_;
    int64_t pos_;

    // from ConnectionInfo(runtime result)
    std::string ip_;
    std::string host_;
    std::string kwai_sign_;
    int response_code_;
    int64_t content_length_;

    // other runtime stats
    int error_;
    int read_len_;

};

}
}
