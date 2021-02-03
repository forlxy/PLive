//
// Created by MarshallShuai on 2019-06-30.
//

#pragma once

#include "data_source_seekable.h"
#include "stats/json_stats.h"
#include "v2/net/scope_curl_http_task.h"
#include "cache_opts.h"
#include "v2/cache/cache_content_v2_with_scope.h"
#include "async_scope_data_source.h"
#include "cache_session_listener.h"

namespace kuaishou {
namespace cache {

class AsyncCacheDataSourceV2 final: public DataSourceSeekable {
  public:
    AsyncCacheDataSourceV2(const DataSourceOpts& opts, std::shared_ptr<CacheSessionListener> listener, AwesomeCacheRuntimeInfo* ac_rt_info);
    ~AsyncCacheDataSourceV2();

    virtual int64_t Open(const DataSpec& spec) override;
    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len) override;
    virtual AcResultType Close() override;

    virtual Stats* GetStats() override {
        return new DummyStats("AsyncCacheDataSourceV2");
    };

    virtual int64_t Seek(int64_t pos) override;

    virtual int64_t ReOpen() override;

  private:
    /**
     * 打开scopeDataSource，并seek到对应的位置
     * @return 永远返回content-length或者负数错误码
     */
    int64_t OpenNextScope();
    void CloseCurrentScope();

  private:
    DataSourceOpts data_source_opts_;
    AwesomeCacheRuntimeInfo* ac_rt_info_;

    DataSpec spec_;
    int context_id_;
    std::shared_ptr<CacheContentV2WithScope> cache_content_;

    std::shared_ptr<CacheScope> current_scope_;
    std::shared_ptr<AsyncScopeDataSource> current_scope_data_source_;

    int32_t last_error_;
    int64_t current_reading_position_;

    std::shared_ptr<CacheSessionListener> cache_session_listener_;
};

} // namespace cache
} // namespace kuaishou


