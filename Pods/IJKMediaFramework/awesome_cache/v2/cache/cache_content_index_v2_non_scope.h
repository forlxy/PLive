//
// Created by MarshallShuai on 2019/10/16.
//
#pragma once

#include "utils/macro_util.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/cache/cache_content_v2_non_scope.h"
#include "v2/cache/cache_v2_settings.h"

HODOR_NAMESPACE_START

class CacheContentIndexV2NonScope : public CacheContentIndexV2<CacheContentV2NonScope> {
  public:
    CacheContentIndexV2NonScope(std::string cache_dir_path);

  protected:
    virtual std::shared_ptr<CacheContentV2NonScope> MakeCacheContent(std::string key,
                                                                     std::string dir_path, int64_t content_length = 0);

    virtual bool CacheContentIntoDataStream(CacheContentV2NonScope& content,
                                            kpbase::DataOutputStream& output_stream);
    virtual std::shared_ptr<CacheContentV2NonScope> CacheContentFromDataStream(kpbase::DataInputStream& input_stream);
};

HODOR_NAMESPACE_END