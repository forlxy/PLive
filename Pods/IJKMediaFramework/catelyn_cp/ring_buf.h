#pragma once

#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <condition_variable>

namespace kuaishou {
namespace kpbase {

namespace {
static const size_t kDefaultCapacity = 1024 * 20; // 20k
}

template<bool with_lock, bool block_push, bool dynamic_adjust = false>
class RingBuf {
 public:
  explicit RingBuf(size_t capacity = kDefaultCapacity) :
    with_lock_(with_lock),
    block_push_(block_push),
    capacity_(capacity),
    mutex_(),
    buf_(nullptr),
    used_size_(0),
    head_(0),
    rear_(0) {
    if (block_push_) {
      with_lock_ = false;
    }
    buf_ = new char[capacity];
  }

  virtual ~RingBuf() {
    Interrupt();
    delete[] buf_;
  }

  size_t PushBack(char* data, size_t len) {
    if (with_lock_) {
      mutex_.lock();
    }
    std::unique_lock<std::mutex> lock(block_mutex_, std::defer_lock);

    if (block_push_) {
      lock.lock();
      not_full_cond_.wait(lock, [&] { return used_size_ + len <= capacity_; });
      if (used_size_ + len > capacity_) {
        if (with_lock) {
          mutex_.unlock();
        }
        return 0;
      }
    }

    // dynamic adjust the capacity
    if (dynamic_adjust && used_size_ + len > capacity_) {
      size_t new_capacity = (capacity_ * 2);
      if (new_capacity < used_size_ + len) {
        new_capacity = used_size_ + len;
      }
      auto used_size = used_size_;
      char* new_buf = new char[new_capacity];
      pop_front_internal(new_buf, used_size_);

      if (buf_) {
        delete[] buf_;
      }
      buf_ = new_buf;
      capacity_ = new_capacity;
      used_size_ = used_size;
      head_ = 0;
      rear_ = static_cast<int>(used_size_);
    }

    if (len >= capacity_) {
      memcpy(buf_, data + (len - capacity_), capacity_);
      head_ = 0;
      rear_ = static_cast<int>(capacity_);
      used_size_ = capacity_;
    } else if (rear_ + len > capacity_) {
      size_t first_part_len = capacity_ - rear_;
      memcpy(buf_ + rear_, data, first_part_len);
      memcpy(buf_, data + first_part_len, len - first_part_len);
      rear_ = static_cast<int>(len - first_part_len);
    } else {
      memcpy(buf_ + rear_, data, len);
      rear_ += len;
    }

    used_size_ += len;
    if (used_size_ > capacity_) {
      head_ = rear_;
      used_size_ = capacity_;
    }

    if (with_lock_) {
      mutex_.unlock();
    }
    return len;
  }

  int PopFront(char* data, size_t len) {
    if (with_lock_) {
      mutex_.lock();
    }

    int ret = pop_front_internal(data, len);

    if (with_lock_) {
      mutex_.unlock();
    }

    return ret;
  }

  void Clear() {
    if (with_lock_) {
      mutex_.lock();
    }
    std::unique_lock<std::mutex> lock(block_mutex_, std::defer_lock);
    if (block_push_) {
      lock.lock();
    }

    used_size_ = 0;
    head_ = 0;
    rear_ = 0;
    memset(buf_, 0, capacity_);
    if (block_push_) {
      not_full_cond_.notify_one();
    }

    if (with_lock_) {
      mutex_.unlock();
    }
  }

  // useful when using push_block
  void Interrupt() {
    if (block_push_) {
      not_full_cond_.notify_one();
    }
  }

  size_t used_size() const {
    return used_size_;
  }

  bool IsEmpty() {
    bool is_empty = true;
    if (with_lock_) {
      mutex_.lock();
    }
    is_empty = used_size_ == 0;
    if (with_lock_) {
      mutex_.unlock();
    }
    return is_empty;
  }

 private:
  int pop_front_internal(char* data, size_t len) {
    std::unique_lock<std::mutex> lock(block_mutex_, std::defer_lock);
    if (block_push_) {
      lock.lock();
    }

    if (used_size_ < len) {
      len = used_size_;
    }

    if (head_ + len > capacity_) {
      size_t first_part_len = capacity_ - head_;
      if (data) {
        memcpy(data, buf_ + head_, first_part_len);
        memcpy(data + first_part_len, buf_, len - first_part_len);
      }
      head_ = static_cast<int>(len - first_part_len);
    } else {
      if (data) {
        memcpy(data, buf_ + head_, len);
      }
      head_ += len;
    }

    used_size_ -= len;
    if (block_push_) {
      not_full_cond_.notify_one();
    }
    return (int)len;
  }

 private:
  bool with_lock_;
  bool block_push_;
  std::mutex mutex_;
  std::mutex block_mutex_;
  std::condition_variable not_full_cond_;

  char* buf_;
  size_t capacity_;
  size_t used_size_;
  int head_;
  int rear_;
};

} // namespace base
} // namespace kuaishou
