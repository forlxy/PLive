#pragma once

#include "sync_cache_data_source_stage_stats.h"
#include "data_spec.h"

namespace kuaishou {
namespace cache {
namespace internal {

#define MAX_SESSION_STAGES 5
#define RETURN_IF_READ_STAGE_LIMIT \
    if (read_stage_cnt_limit_) { \
        return; \
    }


class SyncCacheDataSourceSessionStats : public SessionStats<SyncCacheDataSourceStageStats> {
  public:
    SyncCacheDataSourceSessionStats(const std::string& type) :
        SessionStats<internal::SyncCacheDataSourceStageStats>("SyncSrc"),
        stage_cnt_(0),
        read_stage_cnt_limit_(false) {
    }

    virtual void NewStage() {
        RETURN_IF_DISABLE
        RETURN_IF_READ_STAGE_LIMIT
        if (stage_cnt_ >= MAX_SESSION_STAGES) {
            read_stage_cnt_limit_ = true;
            return;
        }

        SessionStats::NewStage();
        stage_cnt_++;
    }

    void CurrentSaveDataSpec(const DataSpec& spec) {
        RETURN_IF_DISABLE
        RETURN_IF_READ_STAGE_LIMIT
        current_stage()->uri = spec.uri;
        current_stage()->pos = spec.position;
    }

    void CurrentOnError(int error) {
        RETURN_IF_DISABLE
        RETURN_IF_READ_STAGE_LIMIT
        current_stage()->error = error;
    }

    void CurrentOnReadBytes(int64_t len) {
        RETURN_IF_DISABLE
        RETURN_IF_READ_STAGE_LIMIT
        current_stage()->read_total += len;
    }

    void CurrentSetRemainBytes(int64_t len) {
        RETURN_IF_DISABLE
        RETURN_IF_READ_STAGE_LIMIT
        current_stage()->remain_bytes = len;
    }

    void CurrentAppendDataSourceStats(json stat) {
        RETURN_IF_DISABLE
        RETURN_IF_READ_STAGE_LIMIT
        current_stage()->AppendDataSourceStats(stat);
    }


  private:

    int stage_cnt_;
    bool read_stage_cnt_limit_;

    int32_t bytes_remain_;
};

} // namespace internal

}
}
