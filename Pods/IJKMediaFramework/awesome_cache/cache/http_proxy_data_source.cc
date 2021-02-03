#include "http_proxy_data_source.h"
#include <cstdint>
#include "data_spec.h"
#include "connection_info.h"
#include "../download/connection_info.h"
#include "../include/data_spec.h"
#include "../include/awesome_cache_callback.h"
#include "../include/cache_opts.h"

namespace kuaishou {
namespace cache {

HttpProxyDataSource::HttpProxyDataSource(std::shared_ptr<HttpDataSource> upstream,
                                         std::shared_ptr<CacheSessionListener> session_listener,
                                         const DataSourceOpts& opts, AwesomeCacheRuntimeInfo* ac_rt_info) :
    cache_session_listener_(session_listener),
    cache_callback_(static_cast<AwesomeCacheCallback*>(opts.cache_callback)),
    upstream_data_source_(upstream),
    runloop_(new kpbase::Runloop("HttpProxyDataSourceRunLoop")),
    product_context_(opts.download_opts.product_context),
    download_stop_need_to_report_(false),
    data_source_extra_(opts.download_opts.datasource_extra_msg),
    ac_rt_info_(ac_rt_info) {
    callbackInfo_ = std::shared_ptr<AcCallbackInfo>(AcCallbackInfoFactory::CreateCallbackInfo());
}

HttpProxyDataSource::~HttpProxyDataSource() {
    runloop_->PostAndWait([] {});
}

int64_t HttpProxyDataSource::Open(const DataSpec& spec) {
    read_position_ = spec.position;
    int64_t ret = upstream_data_source_->Open(spec);
    HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
    if (hasConnectionInfo) {
        const ConnectionInfo& connectionInfo = hasConnectionInfo->GetConnectionInfo();
        ReportDownloadStarted(read_position_, connectionInfo);
        if (connectionInfo.error_code != 0) {
            ReportDownloadStopped("HttpProxyDataSource::OpenFail", connectionInfo);
        }
    }
    return ret;
}

int64_t HttpProxyDataSource::Read(uint8_t* buf, int64_t offset, int64_t read_len) {
    int64_t bytes_read = upstream_data_source_->Read(buf, offset, read_len);
    if (bytes_read > 0) {
        read_position_ += bytes_read;
    }
    HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
    if (hasConnectionInfo) {
        const ConnectionInfo& connectionInfo = hasConnectionInfo->GetConnectionInfo();
        if (connectionInfo.connection_closed || connectionInfo.IsDownloadComplete()) {
            ReportDownloadStopped("HttpProxyDataSource finish", connectionInfo);
        }
    }
    return bytes_read;
}

AcResultType HttpProxyDataSource::Close() {
    AcResultType res = upstream_data_source_->Close();
    HasConnectionInfo* hasConnectionInfo = dynamic_cast<HasConnectionInfo*>(upstream_data_source_.get());
    if (hasConnectionInfo) {
        ReportDownloadStopped("HttpProxyDataSource::Close", hasConnectionInfo->GetConnectionInfo());
    }
    return res;
}

Stats* HttpProxyDataSource::GetStats() {
    return NULL;
}

void HttpProxyDataSource::ReportDownloadStarted(uint64_t position, const ConnectionInfo& info) {
    LOG_DEBUG("[%d] [HttpProxyDataSource::ReportDownloadStarted] to call OnDownloadStarted, position:%lld \n",
              GetContextId(),  position);
    if (cache_session_listener_) {
        runloop_->Post([ = ] {
            cache_session_listener_->OnDownloadStarted(position, info.uri, info.host, info.ip, info.response_code,
                                                       info.connection_used_time_ms);
        });
        if (cache_callback_) {
            callbackInfo_->SetCurrentUri(info.uri);
            callbackInfo_->SetHost(info.host);
            callbackInfo_->SetIp(info.ip);
            callbackInfo_->SetHttpResponseCode(info.response_code);
            callbackInfo_->SetHttpRedirectCount(info.redirect_count);
            callbackInfo_->SetEffectiveUrl(info.effective_url);
            callbackInfo_->SetKwaiSign(info.sign);
            callbackInfo_->SetXKsCache(info.x_ks_cache);
            callbackInfo_->SetSessionUUID(info.session_uuid);
            callbackInfo_->SetDownloadUUID(info.download_uuid);
            callbackInfo_->SetProductContext(product_context_);
            callbackInfo_->SetContentLength(info.content_length_from_curl_);
        }
        download_stop_need_to_report_ = true;
    }
}

void HttpProxyDataSource::ReportDownloadStopped(const char* tag, const ConnectionInfo& info) {
    if (download_stop_need_to_report_) {
        LOG_DEBUG("[%d] [HttpProxyDataSource::ReportDownloadStopped][tag:%s] to call OnDownloadStopped,"
                  "  info.error_code:%d, stop_reason:%s \n",
                  GetContextId(), tag, info.error_code,
                  CacheSessionListener::DownloadStopReasonToString(info.stop_reason_));
        download_stop_need_to_report_ = false;

        if (cache_session_listener_) {
            runloop_->Post([ = ] {
                cache_session_listener_->OnDownloadStopped(info.stop_reason_,
                                                           info.GetDownloadedBytes(),
                                                           info.transfer_consume_ms_, info.sign,
                                                           info.error_code, info.x_ks_cache,
                                                           info.session_uuid, info.download_uuid, data_source_extra_);
            });

        }
        if (cache_callback_) {
            callbackInfo_->SetDataSourceType(kDataSourceTypeSegment);
            callbackInfo_->SetStopReason(info.stop_reason_);
            callbackInfo_->SetErrorCode(info.error_code);
            callbackInfo_->SetTransferConsumeMs(info.transfer_consume_ms_);
            callbackInfo_->SetDownloadBytes(info.GetDownloadedBytes());
            callbackInfo_->SetRangeRequestStart(info.range_request_start);
            callbackInfo_->SetRangeRequestEnd(info.range_request_end);
            callbackInfo_->SetRangeResponseStart(info.range_response_start);
            callbackInfo_->SetRangeResponseEnd(info.range_response_end);
            callbackInfo_->SetDnsCost(info.http_dns_analyze_ms);
            callbackInfo_->SetConnectCost(info.connection_used_time_ms);
            callbackInfo_->SetFirstDataCost(info.http_first_data_ms);
            callbackInfo_->SetTotalBytes(info.file_length);
            callbackInfo_->SetUpstreamType(ac_rt_info_->cache_applied_config.upstream_type);
            callbackInfo_->SetHttpVersion(ac_rt_info_->download_task.http_version);
            runloop_->Post([ = ] {
                cache_callback_->onDownloadFinish(callbackInfo_);
            });
        }
    }
}

}
}
