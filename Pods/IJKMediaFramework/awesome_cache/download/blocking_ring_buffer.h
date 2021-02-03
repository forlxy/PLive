//
// Created by 帅龙成 on 2018/7/8.
//
#pragma once

#include <stdint.h>
#include <pthread.h>

namespace kuaishou {
namespace cache {

#define BlockingRingBuffer_Default_Capacity_Bytes (2*1024*1024)

enum {
    kBlockingRingBufferOK = 0,
    kBlockingRingBufferInitFail = -1,
    kBlockingRingBufferInterrupted = -2,
    kBlockingRingBufferInvalidArgs = -3,
    kBlockingRingBufferInnerError = -4,
} BlockingRingBufferError;
/**
 *
 */
class BlockingRingBuffer {
  public:
    explicit BlockingRingBuffer(uint32_t capacity = BlockingRingBuffer_Default_Capacity_Bytes);
    ~BlockingRingBuffer();

    /**
     *
     * @param buf buffer pointer
     * @param offset buffer offset
     * @param pop_bytes to_read
     * @return 0 for Pop success, negative for failure, other positive value for actual read bytes
     */
    int32_t PopBuf(uint8_t* buf, int32_t offset, int32_t pop_bytes);

    /**
     *
     * @param buf buffer pointer
     * @param push_bytes 可以push任意大小的buffer，内部会做for循环
     * @return 0 for Push success, negative for failure, no positive value is supposed to be returned
     */
    int32_t PushBuf(uint8_t* buf, int32_t push_bytes, int32_t& used_bytes);

    bool IsEmpty();
    void Interrupt();

  private:
    /**
     *
     * @param buf buffer pointer
     * @param push_bytes push bytes
     * @return for Push success, negative for failure, other positive value for acutual push bytes
     */
    int PushBufPart(uint8_t* buf, int32_t push_bytes, int32_t& used_bytes);

    void SafeMemcpyIn(uint8_t* dst, uint8_t* src, size_t len);
    void SafeMemcpyOut(uint8_t* dst, uint8_t* src, size_t len);
  private:
    bool inited_;
    bool interupted_;

    uint8_t* buf_;
    uint32_t  capacity_;
    int32_t head_offset_, tail_offset_, used_bytes_;


    pthread_mutex_t mutex_;
    pthread_cond_t cond_;

};

}
}


