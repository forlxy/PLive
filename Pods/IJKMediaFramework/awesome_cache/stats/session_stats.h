#pragma once
#include "stats/stage_stats.h"
#include <vector>
#include <type_traits>
#include <atomic>

namespace kuaishou {
namespace cache {

/**
 * For a given data source, the session stats include all the stages of open/close period.
 */
template<class StageClass>
class SessionStats : public JsonStats {
  public:
    virtual ~SessionStats() {
    }

    SessionStats(const std::string type) : JsonStats(type) {
        static_assert(std::is_base_of<StageStats, StageClass>::value, "The template class should"
                      "be derived from SessionStats");
    }

    virtual void NewStage() {
        if (Stats::enabled()) {
            int stage = (int)stages_.size();
            stages_.push_back(std::unique_ptr<StageClass>(new StageClass(stage)));
        }
    }

    StageClass* current_stage() {
        if (Stats::enabled()) {
            if (stages_.size() == 0) {
                return nullptr;
            } else {
                return stages_[stages_.size() - 1].get();
            }
        } else {
            if (!dummy_stage_) {
                dummy_stage_.reset(new StageClass(0));
            }
            return dummy_stage_.get();
        }
    }

  protected:
    virtual void FillJson() override {
        for (auto& stage : stages_) {
            stats_["bStages"].push_back(stage->ToJson());
        }
    }
  private:
    std::vector<std::unique_ptr<StageClass> > stages_;
    std::unique_ptr<StageClass> dummy_stage_;
};

} // namespace cache
} // namespace kuaishou
