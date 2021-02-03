//
// Created by 帅龙成 on 19/10/2017.
//

#include "file_data_source_factory.h"

namespace kuaishou {
namespace cache {

FileDataSourceFactory::FileDataSourceFactory() {
}

FileDataSourceFactory::FileDataSourceFactory(
    std::shared_ptr<TransferListener<FileDataSource>> listener) : listener_(listener) {
}

FileDataSource* FileDataSourceFactory::CreateDataSource() {
    return new FileDataSource(listener_);
}

}
}
