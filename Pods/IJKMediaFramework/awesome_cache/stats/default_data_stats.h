#pragma once
#include "stats/json_stats.h"

namespace kuaishou {
namespace cache {
class DefaultDataStats : public JsonStats {
  public:
    DefaultDataStats(const std::string& type) : JsonStats(type),
        error_code(0),
        bytes_total(0),
        bytes_transfered(0),
        pos(0) {}
    std::string uri;
    int error_code;
    int64_t pos;
    int64_t bytes_total;
    int64_t bytes_transfered;
  protected:
    virtual void FillJson() override {
        // assume fill json is always called after closing data source, so don't lock the members here.
        JsonStats::FillJson();
        stats_["uri"] = uri;
        stats_["error"] = error_code;
        stats_["pos"] = pos;
        stats_["bytes_total"] = bytes_total;
        stats_["bytes_tx"] = bytes_transfered;
    }
};

} // cache
} // kuaishou
