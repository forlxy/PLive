#pragma once
#include <thread>
#include <atomic>

namespace kuaishou {
namespace cache {

class Task {
  public:
    Task() : stop_signal_(false) {}
    virtual ~Task() {
        Cancel();
    }
    void Run() {
        stop_signal_ = false;
        thread_ = std::thread(&Task::RunInternal, this);
    }
    void Cancel() {
        stop_signal_ = true;
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    bool OnTaskThread() {
        return std::this_thread::get_id() == thread_.get_id();
    }

    virtual void LimitCurlSpeed() {
        return;
    };

  protected:
    virtual void RunInternal() = 0;
    std::atomic<bool> stop_signal_;
  private:
    std::thread thread_;
};

}
} // namespace kuaishou::cache
