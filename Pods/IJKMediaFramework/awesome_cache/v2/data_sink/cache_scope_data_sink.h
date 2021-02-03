//
// Created by MarshallShuai on 2019-07-12.
//
#pragma once

#include "data_sink.h"
#include "utils/macro_util.h"
#include "v2/cache/cache_scope.h"
#include "io_stream.h"
#include "file.h"

HODOR_NAMESPACE_START

/**
 * 面向一个Scope的data sink
 * 不支持 position，只支持从头开始write
 */
class CacheScopeDataSink {
  public:
    CacheScopeDataSink(std::shared_ptr<CacheScope> scope);

    AcResultType Open();

    /**
     *
     * @return 实际写入的长度，或者错误值（<0)
     */
    int64_t Write(uint8_t* buf, int64_t offset, int64_t len);

    /**
     * 表示还能往这个scope写多少bytes数据
     * @return 表示还能往这个scope写多少bytes数据
     */
    int64_t GetAvilableScopeRoom();
    void Close();
  private:
    std::shared_ptr<CacheScope> cache_scope_;
    kpbase::File file_;
    std::shared_ptr<kpbase::OutputStream> output_stream_;
    int64_t current_writen_len_;
    AcResultType last_error_;
};

HODOR_NAMESPACE_END
