//
// Created by 帅龙成 on 19/10/2017.
//
#pragma once

#include <memory>
#include "data_source.h"
#include "file_data_source.h"
#include "transfer_listener.h"

namespace kuaishou {
namespace cache {

class FileDataSourceFactory final {

  public:
    FileDataSourceFactory();

    FileDataSourceFactory(std::shared_ptr<TransferListener<FileDataSource>> listener);

    FileDataSource* CreateDataSource();

  private:
    std::shared_ptr<TransferListener<FileDataSource>> listener_;
};

}
}

