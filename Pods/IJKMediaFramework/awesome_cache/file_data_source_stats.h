//
//  file_data_source_stats.h
//  IJKMediaFramework
//
//  Created by 帅龙成 on 2018/8/30.
//  Copyright © 2018 kuaishou. All rights reserved.
//
#pragma once


#include "stats/json_stats.h"
#include "include/data_spec.h"
#include <stdint.h>

namespace kuaishou {
namespace cache {

class FileDataSourceStats : public JsonStats {

  public :
    FileDataSourceStats(const std::string& type):
        JsonStats(type),
        read_total_len_(0),
        error_(0) {
    }

    void  SaveDataSpec(const DataSpec& spec, std::string file_name) {
        RETURN_IF_DISABLE
        file_name_ = file_name;
        pos_ = spec.position;
        spec_len_ = spec.length;
    }

    void OnFileOpened(int64_t file_len) {
        RETURN_IF_DISABLE
        file_len_ = file_len;
    }

    void  OnError(int error) {
        RETURN_IF_DISABLE
        error_ = error;
    }

    void OnReadBytes(int64_t bytes) {
        RETURN_IF_DISABLE
        read_total_len_ += bytes;
    }

  protected:
    virtual void FillJson() {
        RETURN_IF_DISABLE
        stats_["aType"] = type_;
        stats_["bError"] = error_;

        stats_["cFile"] = file_name_;
        stats_["dSpecLen"] = spec_len_;
        stats_["ePos"] = pos_;

        stats_["fFileLen"] = file_len_;
        stats_["fReadTx"] = read_total_len_;
    };

  private:
    // from DataSpec(config)
    std::string file_name_;
    int64_t pos_;
    int64_t spec_len_;

    int64_t file_len_;

    // other runtime stats
    int error_;
    int read_total_len_;

};

}
}
