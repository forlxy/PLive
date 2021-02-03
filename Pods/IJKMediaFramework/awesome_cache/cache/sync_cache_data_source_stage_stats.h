//
//  sync_cache_data_source_stats.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/8/29.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#pragma once

#include "stats/session_stats.h"
#include "data_spec.h"

namespace kuaishou {
namespace cache {
namespace internal {
class SyncCacheDataSourceStageStats : public StageStats {
  public:
    SyncCacheDataSourceStageStats(int stage) : StageStats(stage) {
    }


    void AppendDataSourceStats(json stat) {
        data_source_stats_.push_back(stat);
    }

    std::string uri = "";
    int64_t pos = 0;
    int64_t remain_bytes = 0;
    int64_t read_total = 0;
    int error = 0;

  private:
    virtual void FillJson() override {
        stats_["aType"] = type_;
        stats_["bTs"] = kpbase::SystemUtil::GetTimeString(timestamp_, kTimeZone);
        stats_["cErr"] = error;
        stats_["dUri"] = uri;
        stats_["ePos"] = pos;
        stats_["fRemain"] = remain_bytes;
        stats_["gEndPos"] = pos + remain_bytes;
        stats_["hReadTotal"] = read_total;
        stats_["iDataSrc"] = data_source_stats_;
    }

    json data_source_stats_;

};

} // namespace internal

}
}
