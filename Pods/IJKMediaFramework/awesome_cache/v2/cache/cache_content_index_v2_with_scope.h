//
// Created by MarshallShuai on 2019/10/16.
//
#pragma once

#include "utils/macro_util.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_content_v2_with_scope.h"
#include "v2/cache/cache_v2_settings.h"

HODOR_NAMESPACE_START

class CacheContentIndexV2WithScope : public CacheContentIndexV2<CacheContentV2WithScope> {
  public:
    CacheContentIndexV2WithScope(const std::string& cache_dir_path);

    std::vector<std::shared_ptr<CacheContentV2WithScope>> GetCacheContentListOfEvictStrategy(EvictStrategy strategy);
  protected:
    virtual std::shared_ptr<CacheContentV2WithScope> MakeCacheContent(std::string key,
                                                                      std::string dir_path, int64_t content_length = 0) override;

    virtual bool CacheContentIntoDataStream(CacheContentV2WithScope& content,
                                            kpbase::DataOutputStream& output_stream) override;
    virtual std::shared_ptr<CacheContentV2WithScope> CacheContentFromDataStream(kpbase::DataInputStream& input_stream) override;
};


HODOR_NAMESPACE_END
