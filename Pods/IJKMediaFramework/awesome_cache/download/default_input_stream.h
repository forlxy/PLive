#pragma once

#include <atomic>
#include "download/input_stream.h"
#include "ring_buf.h"
namespace kuaishou {
namespace cache {

/**
 * Default input stream is a stream with a container that will block the write operation when container is full,
 * and will be unblocked till a read operation.
 */
class DefaultInputStream : public InputStream {
  public:
    DefaultInputStream(size_t capacity);

    virtual ~DefaultInputStream();

    virtual int Read(uint8_t* buf, int32_t offset, int32_t len) override;

    virtual bool HasMoreData() override;

    virtual int error_code() override;

    virtual void Close();

    void Reset();

    void FeedDataSync(uint8_t* data, int32_t len);

    /**
     * End of Stream, called when there's no more data feeding into container.
     * If there's no error, send 0 as error code.
     */
    void EndOfStream(int error_code);

  private:
    kpbase::RingBuf<true, true> container_;
    std::atomic<bool> close_signaled_;
    std::atomic<bool> eof_signaled_;
    std::atomic<int> error_code_;
    size_t capacity_;
};

} // cache
} // kuaishou
