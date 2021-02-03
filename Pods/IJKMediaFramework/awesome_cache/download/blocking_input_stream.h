//
// Created by 帅龙成 on 2018/7/8.
//
#pragma once

#include <memory>
#include "download/input_stream.h"
#include "blocking_ring_buffer.h"
namespace kuaishou {
namespace cache {

class BlockingInputStream : public InputStream {

  public:
    explicit BlockingInputStream(uint32_t capacity);

    virtual int Read(uint8_t* buf, int32_t offset, int32_t len) override ;

    virtual bool HasMoreData() override ;

    virtual int error_code() override ;

    void Close();

//  void Reset();

    int FeedDataSync(uint8_t* data, int32_t len, int32_t& used_bytes);

    /**
     * End of Stream, called when there's no more data feeding into container.
     * If there's no error, send 0 as error code.
     */
    void EndOfStream(int error_code);
  private:
    std::unique_ptr<BlockingRingBuffer> ring_buffer_;
    uint32_t capacity_;
    bool close_signaled_;
    bool eof_signaled_;
    int error_code_;
};

}
}


