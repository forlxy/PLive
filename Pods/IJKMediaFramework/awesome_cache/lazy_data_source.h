//#pragma once
//#include "data_source.h"
//#include "http_data_source_factory.h"
//#include "file_data_source_factory.h"
//#include "constant.h"
//#include "utility.h"
//
//namespace kuaishou {
//namespace cache {
///**
// * A data source that initializes lazily when opening according to its spec uri.
// */
//class LazyDataSource : public DataSource {
//  public:
//    LazyDataSource(std::shared_ptr<FileDataSourceFactory> file_factory,
//                   std::shared_ptr<HttpDataSourceFactory> http_factory) :
//        file_factory_(file_factory),
//        http_factory_(http_factory) {
//    }
//
//    int64_t Open(const DataSpec& spec) override {
//        if (kpbase::StringUtil::StartWith(spec.uri, kHttpProtocolPrefix)) {
//            data_source_.reset(http_factory_->CreateDataSource(DownloadOpts(), ));
//        } else if (kpbase::StringUtil::StartWith(spec.uri, kFileProtocolPrefix) || kpbase::StringUtil::StartWith(spec.uri, "/")) {
//            data_source_.reset(file_factory_->CreateDataSource());
//        }
//        if (data_source_) {
//            return data_source_->Open(spec);
//        }
//        return -1;
//    }
//
//    int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override {
//        if (data_source_) {
//            return data_source_->Read(buf, offset, len);
//        }
//        return -1;
//    }
//
//    AcResultType Close() override {
//        if (data_source_) {
//            return data_source_->Close();
//        }
//        return kResultOK;
//    }
//
//    Stats* GetStats() override {
//        return data_source_ ? data_source_->GetStats() : nullptr;
//    }
//
//  private:
//    std::unique_ptr<DataSource> data_source_;
//    std::shared_ptr<FileDataSourceFactory> file_factory_;
//    std::shared_ptr<HttpDataSourceFactory> http_factory_;
//};
//
//} // cache
//} // kuaishou
