#pragma once
#include <string>

namespace kuaishou {
namespace cache {

class Stats {
  public:
    virtual ~Stats() {
    }

    static void EnableStats(bool enable) {
        enabled_ = enable;
    }
    static bool enabled() {
        return enabled_;
    }
    virtual std::string ToString() = 0;
  private:
    static bool enabled_;
};

} // namespace cache
} // namespace kuaishou
