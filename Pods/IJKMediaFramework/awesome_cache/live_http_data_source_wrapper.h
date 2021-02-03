#ifndef AWESOME_CACHE_LIVE_HTTP_DATASOURCE_WRAPPER_H_
#define AWESOME_CACHE_LIVE_HTTP_DATASOURCE_WRAPPER_H_ value


#include <curl/curl.h>
#include "http_data_source.h"


namespace kuaishou {
namespace cache {

class LiveHttpDataSourceWrapper final: public HttpDataSource {

    // 这个datasource将http datasource包一层，用于直播
    // 对直播来说，http datasource以上没有cache datasource
    // 其返回值是直接传到ffmpeg adapter层的，有一些错误码需要处理，使得与原有ffmpeg行为兼容

  public:
    LiveHttpDataSourceWrapper(std::unique_ptr<HttpDataSource>&& ds): ds_(std::move(ds)) {}

    virtual int64_t Open(const DataSpec& spec) override {
        if (!ds_) {
            return kResultExceptionSourceNotOpened_0;
        }
        return ds_->Open(spec);
    }

    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override {
        // Note: ffmpeg_adapter would rewrite all error codes except kResultEndOfInput to AVERROR_EXIT
        // Also, AVERROR_EXIT is not a critical error to APP while AVERROR_EOF is
        // So change all error codes except USER EXIT to kResultEndOfInput here
        if (!ds_) {
            return kResultExceptionSourceNotOpened_1;
        }
        auto ret = ds_->Read(buf, offset, len);

        if (ret == kResultExceptionHttpDataSourceReadNoData) {
            // 与ffmpeg行为对齐，直播下ffurl_read返回0时我返回kResultLiveNoData，再由ffmpeg adapter转为0
            return kResultLiveNoData;
        }
        if (ret == kResultFFurlExit || ret == kLibcurlErrorBase + (-CURLE_ABORTED_BY_CALLBACK)) {
            return ret;
        }
        if (ret < 0) {
            return kResultEndOfInput;
        }
        return ret;
    }

    virtual AcResultType Close() override {
        if (!ds_) {
            return kResultExceptionSourceNotOpened_2;
        }
        return ds_->Close();
    }

    virtual Stats* GetStats() override {
        if (!ds_) {
            return nullptr;
        }
        return ds_->GetStats();
    }

  private:
    std::unique_ptr<HttpDataSource> ds_;

};


}
}

#endif /* ifndef AWESOME_CACHE_LIVE_HTTP_DATASOURCE_WRAPPER_H_ */
