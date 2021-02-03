#include "default_input_stream.h"

namespace kuaishou {
namespace cache {

DefaultInputStream::DefaultInputStream(size_t capacity) :
    container_(capacity),
    capacity_(capacity),
    error_code_(0),
    eof_signaled_(false),
    close_signaled_(false) {
}

DefaultInputStream::~DefaultInputStream() {
    Close();
}

int DefaultInputStream::Read(uint8_t* buf, int32_t offset, int32_t len) {
    return container_.PopFront((char*)(buf + offset), len);
}

void DefaultInputStream::FeedDataSync(uint8_t* data, int32_t len) {
    while (!close_signaled_ && len) {
        int32_t len_to_feed = len > capacity_ ? (int32_t)capacity_ : len;
        size_t actual = container_.PushBack((char*)data, len_to_feed);
        data += actual;
        len -= actual;
    }

}

bool DefaultInputStream::HasMoreData() {
    bool no_more_data = eof_signaled_ && container_.IsEmpty();
    return !no_more_data;
}

int DefaultInputStream::error_code() {
    return error_code_;
}

void DefaultInputStream::Close() {
    close_signaled_ = true;
    container_.Interrupt();
    container_.Clear();
}

void DefaultInputStream::Reset() {
    error_code_ = 0;
    container_.Clear();
    eof_signaled_ = false;
    close_signaled_ = false;
}

void DefaultInputStream::EndOfStream(int error_code) {
    error_code_ = error_code;
    eof_signaled_ = true;
}


}
}
