//
//  file_writer_memory_data_source.h
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/11/22.
//  Copyright © 2017年 kuaishou. All rights reserved.
//
#pragma once
#include <algorithm>
#include "data_source.h"
#include "file.h"
#include "buffered_output_stream.h"
#include "constant.h"
#include "stats/default_data_stats.h"

namespace kuaishou {
namespace cache {

class FileWriterMemoryDataSource : public DataSource {
  public:
    FileWriterMemoryDataSource(kpbase::File file, int64_t position, int32_t max_file_size = kDefaultMaxCacheFileSize);

    virtual ~FileWriterMemoryDataSource();

    virtual int64_t Open(const DataSpec& spec) override;

    int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len) override;

    virtual AcResultType Close() override;

    virtual Stats* GetStats() override;

    Stats* GetWriteStats();

    int64_t Write(uint8_t* buf, int64_t offset, int64_t len);

    uint8_t* Buf();

    bool InFileWriterMemory(int64_t read_position);

  private:
    std::unique_ptr<kpbase::BufferedOutputStream> output_stream_;
    uint8_t* buf_;
    int64_t position_;
    int32_t max_file_size_;
    int32_t write_offset_;
    int32_t read_offset_;
    int32_t file_size_;
    kpbase::File file_;
    std::unique_ptr<DefaultDataStats> write_stats_;
    std::unique_ptr<DefaultDataStats> read_stats_;
};

} // namespace cache
} // namespace kuaishou
