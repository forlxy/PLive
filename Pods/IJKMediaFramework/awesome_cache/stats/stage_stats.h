#pragma once
#include "stats/json_stats.h"
#include "utility.h"

namespace kuaishou {
namespace cache {

/**
 * For a given data source, the period between an open/close pair is called a stage.
 */
class StageStats : public JsonStats {
  public:
    StageStats(int stage) :
        stage_(stage),
        JsonStats("st-") {
        type_.append(kpbase::StringUtil::Int2Str(stage));
    }

    int stage() {return stage_;}
  private:
    const int stage_;
};

} // namespace cache
} // namespace kuaishou
