#pragma once
#include <json/json.h>
#include <stdint.h>
#include <atomic>
#include "stats.h"
#include "utility.h"
#include "constant.h"

using json = nlohmann::json;

namespace kuaishou {
namespace cache {

class JsonStats : public Stats {
  public:
    virtual std::string ToString() override {
        FillJson();
        return Stats::enabled() ? stats_.dump() : "{}";
    }

    json ToJson() {
        FillJson();
        return stats_;
    }

  protected:
    JsonStats(const std::string& type) :
        type_(type),
        timestamp_(kpbase::SystemUtil::GetEpochTime()) {
    }

    virtual void FillJson() {};

    std::string GetTimeStamp() {
        return kpbase::SystemUtil::GetTimeString(timestamp_, kTimeZone);
    }

    int64_t timestamp_;
    std::string type_;
    json stats_;
};

class DummyStats : public JsonStats {

  public :
    DummyStats(const std::string& type) :
        JsonStats(type) {
    }
};

} // namespace cache
} // namespace kuaishou
