//
// Created by MarshallShuai on 2019-07-12.
//

#pragma once

#include <vector>
#include "data_sink.h"
#include "utils/macro_util.h"
#include "v2/cache/cache_content_v2.h"
#include "v2/data_sink/cache_scope_data_sink.h"

HODOR_NAMESPACE_START

/**
 * Cache V2系统的sink
 */
class CacheContentV2WithScopeDataSink : public DataSink {
  public:
    CacheContentV2WithScopeDataSink(std::shared_ptr<CacheContentV2WithScope> content);

    /**
     * 不支持spec.position != 0的case
     * @param spec DataSpec
     * @return Open返回值
     */
    virtual int64_t Open(const DataSpec& spec) override;

    virtual int64_t Write(uint8_t* buf, int64_t offset, int64_t len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

  private:
    AcResultType OpenNextScopeSink();
    AcResultType CloseCurrentScopeSink();

  private:
    std::shared_ptr<CacheContentV2WithScope> cache_content_;
    int64_t current_writen_len_;
    AcResultType last_error_;
    std::shared_ptr<CacheScope> current_scope_;
    std::shared_ptr<CacheScopeDataSink> current_scope_sink_;
};

HODOR_NAMESPACE_END
