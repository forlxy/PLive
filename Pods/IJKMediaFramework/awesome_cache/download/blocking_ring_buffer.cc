//
// Created by 帅龙成 on 2018/7/8.
//

#include <algorithm>
#include "ac_log.h"
#include "blocking_ring_buffer.h"
#include "assert.h"
namespace kuaishou {
namespace cache {

static const bool VERBOSE = false;
static const bool DEBUG_CHECK_VALIDITY = false;

BlockingRingBuffer::BlockingRingBuffer(uint32_t capacity):
    interupted_(false),
    capacity_(capacity),
    head_offset_(0),
    tail_offset_(0),
    used_bytes_(0) {
    inited_ = true;
    buf_ = new uint8_t[capacity];
    if (!buf_) {
        inited_ = false;
    }

    if (pthread_mutex_init(&mutex_, NULL) != 0) {
        inited_ = false;
    }

    if (pthread_cond_init(&cond_, NULL) != 0) {
        inited_ = false;
    }
}

BlockingRingBuffer::~BlockingRingBuffer() {
    if (buf_) {
        delete []buf_;
    }
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&cond_);

}

int32_t BlockingRingBuffer::PopBuf(uint8_t* buf, int32_t offset, int32_t pop_bytes) {
    if (!inited_ || offset < 0 || pop_bytes <= 0) {
        return kBlockingRingBufferInitFail;
    }
    if (!buf || offset < 0) {
        return kBlockingRingBufferInvalidArgs;
    }
    int ret = 0;

    pthread_mutex_lock(&mutex_);
    for (;;) {
        if (VERBOSE) {
            LOG_DEBUG("[BlockingRingBuffer::PopBuf][%p],offset:%d, pop_bytes:%d, head_offset_:%d, tail_offset_:%d, used_bytes_:%d \n",
                      this, offset, pop_bytes, head_offset_, tail_offset_, used_bytes_);
        }

        const int32_t to_read = std::min(pop_bytes, used_bytes_);


        if (head_offset_ < tail_offset_) {
            // head_offset_ < tail_offset_, has data
            SafeMemcpyOut(buf + offset, buf_ + head_offset_, static_cast<size_t>(to_read));
            head_offset_ += to_read;

            used_bytes_ -= to_read;
            ret = to_read;
            break;
        } else if (head_offset_ == tail_offset_ && used_bytes_ == 0) {
            // has no data
            if (interupted_) {
                ret = kBlockingRingBufferInterrupted;
                break;
            } else {
                pthread_cond_wait(&cond_, &mutex_);
            }
        } else if (head_offset_ == tail_offset_ && used_bytes_ != capacity_) {
            // invalid state
            ret = kBlockingRingBufferInnerError;
            break;
        } else {
            // tail_offset_ > tail_offset_, do 'ring pop'
            if (capacity_ - head_offset_ >= to_read) {
                SafeMemcpyOut(buf + offset, buf_ + head_offset_, static_cast<size_t>(to_read));

                head_offset_ += to_read;
                head_offset_ = head_offset_ % capacity_;
            } else {
                size_t part1_len = capacity_ - head_offset_;
                SafeMemcpyOut(buf + offset, buf_ + head_offset_, part1_len);
                size_t part2_len = to_read - part1_len;
                SafeMemcpyOut(buf + offset + part1_len, buf_, part2_len);
                head_offset_ = (int32_t)part2_len;

                assert(head_offset_ <= tail_offset_);
            }

            used_bytes_ -= to_read;
            ret = to_read;
            break;
        }
    }


    if (DEBUG_CHECK_VALIDITY) {
        // assert validity
        if (head_offset_ == tail_offset_) {
            assert(used_bytes_ == 0 || used_bytes_ == capacity_);
        } else if (head_offset_ < tail_offset_) {
            assert(used_bytes_ == tail_offset_ - head_offset_);
        } else {
            // head_offset_ > tail_offset_
            assert(used_bytes_ == (capacity_ - head_offset_ + tail_offset_));
        }
    }

    if (ret > 0) {
        pthread_cond_signal(&cond_);
    }

    pthread_mutex_unlock(&mutex_);

    if (VERBOSE) {
        LOG_DEBUG("[BlockingRingBuffer::PopBuf], return :%d bytes \n", ret);
    }
    return ret;
}


int32_t BlockingRingBuffer::PushBufPart(uint8_t* buf, int32_t push_bytes, int32_t& used_bytes) {
    int ret = kBlockingRingBufferOK;

    pthread_mutex_lock(&mutex_);
    for (;;) {
        if (interupted_) {
            ret = kBlockingRingBufferInterrupted;
            break;
        }

        const int32_t to_write = std::min(push_bytes, (int32_t)(capacity_ - used_bytes_));

        if (VERBOSE) {
            LOG_DEBUG("[BlockingRingBuffer::PushBufPart][%p], push_bytes:%d, head_offset_:%d, tail_offset_:%d, used_bytes_:%d to_write:%d, \n",
                      this, push_bytes, head_offset_, tail_offset_, used_bytes_, to_write);
        }

        if (head_offset_ > tail_offset_) {
            // has room
            SafeMemcpyIn(buf_ + tail_offset_, buf, static_cast<size_t>(to_write));
            tail_offset_ += to_write;
            assert(tail_offset_ <= head_offset_);

            used_bytes_ += to_write;

            ret = to_write;
            break;
        } else if (head_offset_ == tail_offset_ && used_bytes_ == capacity_) {
            // has no room
            pthread_cond_wait(&cond_, &mutex_);
        } else if (head_offset_ == tail_offset_ && used_bytes_ != 0) {
            // invalid state
            LOG_ERROR_DETAIL("[BlockingRingBuffer::PushBufPart] kBlockingRingBufferInnerError");
            ret = kBlockingRingBufferInnerError;
            break;
        } else {
            // head_offset_ < tail_offset_ , to do "ring push"
            if (capacity_ - tail_offset_ >= to_write) {
                SafeMemcpyIn(buf_ + tail_offset_, buf, static_cast<size_t>(to_write));
                tail_offset_ += to_write;
                tail_offset_ = tail_offset_ % capacity_;
            } else {
                size_t part1_len = capacity_ - tail_offset_;
                SafeMemcpyIn(buf_ + tail_offset_, buf, part1_len);
                size_t part2_len = to_write - part1_len;
                SafeMemcpyIn(buf_, buf + part1_len, part2_len);
                tail_offset_ = (int32_t)part2_len;
            }

            used_bytes_ += to_write;
            ret = to_write;
            break;
        }
    }

    if (DEBUG_CHECK_VALIDITY) {
        // assert validity
        if (head_offset_ == tail_offset_) {
            assert(used_bytes_ == 0 || used_bytes_ == capacity_);
        } else if (head_offset_ < tail_offset_) {
            assert(used_bytes_ == tail_offset_ - head_offset_);
        } else {
            // head_offset_ > tail_offset_
            assert(used_bytes_ == (capacity_ - head_offset_ + tail_offset_));
        }
    }
    if (VERBOSE) {
        LOG_DEBUG("[BlockingRingBuffer::PushBufPart], pushed :%d bytes \n", ret);
    }

    used_bytes = this->used_bytes_;
    if (ret > 0) {
        pthread_cond_signal(&cond_);
    }


    pthread_mutex_unlock(&mutex_);

    return ret;
}

int32_t BlockingRingBuffer::PushBuf(uint8_t* buf, int32_t push_bytes, int32_t& used_bytes) {
    if (!inited_) {
        LOG_ERROR_DETAIL("[BlockingRingBuffer::PushBuf], kBlockingRingBufferInitFail");
        return kBlockingRingBufferInitFail;
    }
    if (!buf || push_bytes <= 0) {
        LOG_ERROR_DETAIL("[BlockingRingBuffer::PushBuf], kBlockingRingBufferInvalidArgs");
        return kBlockingRingBufferInvalidArgs;
    }

    int total_pushed_bytes = 0;

    while (total_pushed_bytes < push_bytes) {
        int ret = PushBufPart(buf + total_pushed_bytes, push_bytes - total_pushed_bytes, used_bytes);
        if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            LOG_ERROR_DETAIL("[BlockingRingBuffer::PushBuf], kBlockingRingBufferInnerError");
            return kBlockingRingBufferInnerError;
        } else {
            total_pushed_bytes += ret;
        }
    }

    return kBlockingRingBufferOK;
}

bool BlockingRingBuffer::IsEmpty() {
    bool ret = false;
    pthread_mutex_lock(&mutex_);
    ret = (used_bytes_ == 0);
    pthread_mutex_unlock(&mutex_);
    return ret;
}

void BlockingRingBuffer::Interrupt() {

    pthread_mutex_lock(&mutex_);
    interupted_ = true;
    pthread_cond_signal(&cond_);
    pthread_mutex_unlock(&mutex_);
}


void BlockingRingBuffer::SafeMemcpyIn(uint8_t* dst, uint8_t* src, size_t len) {
    if (DEBUG_CHECK_VALIDITY) {
        assert(dst + len <= buf_ + capacity_);
        assert(dst >= buf_);
    }
    memcpy(dst, src, len);
}

void BlockingRingBuffer::SafeMemcpyOut(uint8_t* dst, uint8_t* src, size_t len) {
    if (DEBUG_CHECK_VALIDITY) {
        assert(src + len <= buf_ + capacity_);
        assert(src >= buf_);
    }
    memcpy(dst, src, len);
}
}
}
