#include <include/awesome_cache_runtime_info_c.h>
#include "tee_data_source.h"
#include "ac_log.h"

namespace kuaishou {
namespace cache {

static const int kFlushOnCloseBufferSize = 64 * 1024;

#define RETURN_ERROR(error) \
    { \
        stats_->OnError(error); \
        return error; \
    }

TeeDataSourceStats::TeeDataSourceStats(DataSource* source, DataSink* sink) :
    JsonStats("TeeSrc"),
    source_(source),
    sink_(sink) {}

void TeeDataSourceStats::FillJson() {
    stats_["aType"] = type_;
    JsonStats* source_stats = static_cast<JsonStats*>(source_->GetStats());
    JsonStats* sink_stats = static_cast<JsonStats*>(sink_->GetStats());
    stats_["bSrc"] = source_stats ? source_stats->ToJson() : "(null)";
    stats_["cSink"] = sink_stats ? sink_stats->ToJson() : "(null)";
}

TeeDataSource::TeeDataSource(std::shared_ptr<DataSource> source, std::shared_ptr<DataSink> sink,
                             AwesomeCacheRuntimeInfo* ac_rt_info) :
    source_(source),
    sink_(sink),
    ac_rt_info_(ac_rt_info) {

    ac_rt_info_->tee_ds.sink_write_cost_ms = 0;
}

int64_t TeeDataSource::Open(const DataSpec& spec) {
    stats_.reset(new TeeDataSourceStats(source_.get(), sink_.get()));
    spec_ = spec;
    int64_t ret = source_->Open(spec);
    if (ret < 0) {
        // error opening upstream source.
        LOG_ERROR("TeeDataSource::Open, source_->Open error, ret:%d", ret);
        return ret;
    } else {
        read_bytes_total_ = 0;
    }
    int64_t length = ret;
    DataSpec sink_spec = spec;
    if (spec.length == kLengthUnset && length != kLengthUnset) {
        sink_spec = sink_spec.WithLength(length);
    }
    ret = sink_->Open(sink_spec);
    if (ret < 0) {
        LOG_ERROR("TeeDataSource::Open, sink_->Open error, ret:%d", ret);
        return ret;
    }
    return length;
}

int64_t TeeDataSource::Read(uint8_t* buf, int64_t offset, int64_t len) {
    if (!source_) {
        return kResultExceptionSourceNotOpened_3;
    }

    // LOG_DEBUG("[TeeDataSource][%s], offset:%lld, len:%lld", __func__, offset, len);
    int64_t read_len = source_->Read(buf, offset, len);
    if (read_len > 0) {
        uint64_t start = kpbase::SystemUtil::GetCPUTime();
        int64_t ret = sink_->Write(buf, offset, read_len);
        uint64_t end = kpbase::SystemUtil::GetCPUTime();
        ac_rt_info_->tee_ds.sink_write_cost_ms += (int)(end - start);
        read_bytes_total_ += read_len;
//    LOG_DEBUG("[sink_cost] [Tee] sink_write_cost_ms_:%dms single:%dms, start:%llu,  end:%llu, <<<< \n", sink_write_cost_ms_, (int)(end - start), start, end);
        // For the reason that the read operation has already taken effect, so we defer the error report
        // the next time.
        if (ret < 0) {
            LOG_ERROR_DETAIL("[TeeDataSource] sink write failed %lld", ret);
            return ret;
        }
    } else if (read_len == 0) {
        LOG_ERROR_DETAIL("[TeeDataSource::Read], read_len = 0");
        return kResultTeeDataSinkInnerError;
    }

    return read_len;
}

AcResultType TeeDataSource::Close() {
    AcResultType ret = source_->Close();


    int64_t flush_bytes = 0, flush_bytes_total = 0;
    // 虽然DefaultHttpDataSource 无条件返回kResultOK，但是出于逻辑上的完善性，这里还是判断一下ret
    if (ret == kResultOK) {
        uint8_t flush_buf[kFlushOnCloseBufferSize];
        while ((flush_bytes = Read(flush_buf, 0, kFlushOnCloseBufferSize)) > 0) {
            flush_bytes_total += flush_bytes;
            read_bytes_total_ += flush_bytes;
        }
    }

    LOG_DEBUG("[TeeDataSource::Close] spec_.position:%lld, spec_.length:%lld, read_bytes_total_：%lld, "
              "flush_bytes_total: %d  :), last flush ret:%lld \n",
              spec_.position, spec_.length, read_bytes_total_, flush_bytes_total, flush_bytes);

    ret = sink_->Close();
    return ret;
}

void TeeDataSource::LimitCurlSpeed() {
    if (source_) {
        source_->LimitCurlSpeed();
    }
}

Stats* TeeDataSource::GetStats() {
    return stats_.get();
}


const ConnectionInfo& TeeDataSource::GetConnectionInfo() {
    HasConnectionInfo* hasConInfo = dynamic_cast<HasConnectionInfo*>(source_.get());
    if (hasConInfo) {
        return hasConInfo->GetConnectionInfo();
    } else {
        LOG_ERROR_DETAIL("[TeeDataSource::GetConnectionInfo] WARNING: source_ has not implemented HasConnectionInfo");
        assert(0);
    }
}

#undef RETURN_ERROR

} // cache
} // kuaishou

