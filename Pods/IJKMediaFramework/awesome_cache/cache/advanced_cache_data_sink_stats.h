//
//  advanced_cache_data_sink_stats.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/8/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#pragma once

#include "stats/json_stats.h"

namespace kuaishou {
namespace cache {
namespace internal {

class AdvancedCacheDataSinkStats : public JsonStats {
  public:
    struct StatSpan {
        bool locked;
        std::string uri;
        int64_t pos;
        int64_t length;
        json ToJson() {
            return json{
                {"uri", uri},
                {"locked", locked},
                {"pos", pos},
                {"len", length},
            };
        };
    };

  public:
    AdvancedCacheDataSinkStats()
        : JsonStats("AdvSink"),
          error_(0),
          w_total_len_(0) {}

    void OnError(int error) {
        RETURN_IF_DISABLE
        error_ = error;

    }

    void  SaveDataSpec(const DataSpec& spec) {
        RETURN_IF_DISABLE
        pos_ = spec.position;
        spec_len_ = spec.length;
    }

    void StartSpan(bool locked, std::string uri, int64_t position) {
        RETURN_IF_DISABLE
        spans_.push_back(StatSpan{
            .uri = uri,
            .locked = locked,
            .length = 0,
            .pos = position,
        });
    }

    void OnByteWritten(int64_t len) {
        RETURN_IF_DISABLE
        if (spans_.size() > 0) {
            spans_[spans_.size() - 1].length += len;
        }
        w_total_len_ += len;
    }

  private:
    virtual void FillJson() override {
        RETURN_IF_DISABLE
        stats_["aType"] = type_;
        stats_["bErr"] = error_;
        stats_["cWriteTx"] = w_total_len_;
        for (auto& span : spans_) {
            stats_["dSpans"].push_back(span.ToJson());
        }
    }

    std::vector<StatSpan> spans_;
    int error_;
    int64_t pos_;
    int64_t spec_len_;
    int32_t w_total_len_;
};

} // internal
}
}
