#include "ffurl_http_data_source.h"

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/dict.h"
#include "libavutil/log.h"

#ifdef __cplusplus
}
#endif

using namespace kuaishou;
using namespace kuaishou::cache;

// convert AVERROR to kResult*
static AcResultType avio_error_to_ac_error(int err) {
    switch (err) {
        case AVERROR_PROTOCOL_NOT_FOUND:
            return kResultFFurlProtocolNotFound;
        case AVERROR_EIO:
        case AVERROR(EIO):
            return kResultFFurlIOError;
        case AVERROR(ETIMEDOUT):
            return kResultFFurlTimeout;
        case AVERROR_HTTP_BAD_REQUEST:
        case AVERROR_HTTP_UNAUTHORIZED:
        case AVERROR_HTTP_FORBIDDEN:
        case AVERROR_HTTP_NOT_FOUND:
        case AVERROR_HTTP_OTHER_4XX:
            return kResultFFurlHttp4xx;
        case AVERROR_HTTP_SERVER_ERROR:
            return kResultFFurlHttp5xx;
        case AVERROR_INVALIDDATA:
            return kResultFFurlInvalidData;
        case AVERROR_EXIT:
            return kResultFFurlExit;
        case AVERROR_EOF:
            return kResultEndOfInput;
        default:
            return kResultFFurlUnknown;
    }
}

static int avio_interrupt_cb_wrapper(void* p) {
    return AwesomeCacheInterruptCB_is_interrupted(static_cast<AwesomeCacheInterruptCB*>(p)) ? 1 : 0;
}


FFUrlHttpDataSource::FFUrlHttpDataSource(DownloadOpts const& opts,
                                         AwesomeCacheRuntimeInfo* ac_rt_info):
    opts_(opts), ac_rt_info_(ac_rt_info) {
    this->avio_interrupt_cb_.callback = avio_interrupt_cb_wrapper;
    this->avio_interrupt_cb_.opaque = const_cast<AwesomeCacheInterruptCB*>(&this->opts_.interrupt_cb);
}

int64_t FFUrlHttpDataSource::Open(DataSpec const& spec) {
    this->timestamp_start_download_ = kpbase::SystemUtil::GetCPUTime();

    AVDictionary* format_opts = nullptr;

    char opt_valstr[64];

    snprintf(opt_valstr, 64, "%ld", this->opts_.read_timeout_ms * 1000L);  // "timeout" is in microsecond
    av_dict_set(&format_opts, "timeout", opt_valstr, 0);

    snprintf(opt_valstr, 64, "%d", this->opts_.connect_timeout_ms);
    av_dict_set(&format_opts, "open_timeout", opt_valstr, 0);

    if (spec.position != kLengthUnset && spec.length > 0) {
        snprintf(opt_valstr, 64, "%lld", spec.position);
        av_dict_set(&format_opts, "offset", opt_valstr, 0);
    }

    if (spec.position != kLengthUnset && spec.length != kLengthUnset) {
        snprintf(opt_valstr, 64, "%lld", spec.position + spec.length);
        av_dict_set(&format_opts, "end_offset", opt_valstr, 0);
    }

    av_dict_set(&format_opts, "headers", this->opts_.headers.c_str(), 0);
    av_dict_set(&format_opts, "user_agent", this->opts_.user_agent.c_str(), 0);

    int ret = ffurl_open_whitelist(&this->ctx_, spec.uri.c_str(),
                                   AVIO_FLAG_READ,
                                   &this->avio_interrupt_cb_,
                                   &format_opts,
                                   "http,https,tcp,tls");

#define GET_RESULT_FORMAT_OPTS_INT(NAME, KEY) \
    do { \
        AVDictionaryEntry* entry = nullptr; \
        if ((entry = av_dict_get(format_opts, KEY, NULL, 0))) \
            if (entry->value) \
                this->connection_info_.NAME = std::atoi(entry->value); \
    } while(0)

#define GET_RESULT_FORMAT_OPTS_STR(NAME, KEY) \
    do { \
        AVDictionaryEntry* entry = nullptr; \
        if ((entry = av_dict_get(format_opts, KEY, NULL, 0))) \
            if (entry->value) \
                this->connection_info_.NAME = std::string(entry->value); \
    } while(0)

    GET_RESULT_FORMAT_OPTS_INT(content_length, "http_content_length");
    // compatible with curl download task
    GET_RESULT_FORMAT_OPTS_INT(content_length_from_curl_, "http_content_length");
    GET_RESULT_FORMAT_OPTS_INT(response_code, "http_code");
    GET_RESULT_FORMAT_OPTS_INT(connection_used_time_ms, "connect_time");
    GET_RESULT_FORMAT_OPTS_INT(http_dns_analyze_ms, "analyze_dns_time");
    GET_RESULT_FORMAT_OPTS_INT(http_first_data_ms, "first_data_time");

    this->connection_info_.uri = spec.uri;
    if (!this->opts_.headers.empty()) {
        auto headers = kpbase::StringUtil::Split(this->opts_.headers, "\r\n");
        for (std::string& header : headers) {
            auto trimmed = kpbase::StringUtil::Trim(header);
            auto key_value_vec = kpbase::StringUtil::Split(header, ":");
            if (key_value_vec.size() == 2) {
                auto key = kpbase::StringUtil::Trim(key_value_vec[0]);
                auto value = kpbase::StringUtil::Trim(key_value_vec[1]);
                if (key == "Host" || key == "host") {
                    this->connection_info_.host = value;
                }
            }
        }
    }
    if (this->ctx_) {
        this->connection_info_.ip = ff_qytcp_get_ip(qyhttp_get_tcpstream(this->ctx_));
    }
    GET_RESULT_FORMAT_OPTS_STR(ip, "server_ip");
    GET_RESULT_FORMAT_OPTS_STR(sign, "kwaisign");
    GET_RESULT_FORMAT_OPTS_STR(x_ks_cache, "x_ks_cache");

    strncpy(ac_rt_info_->download_task.kwaisign, this->connection_info_.sign.c_str(), CDN_KWAI_SIGN_MAX_LEN);
    strncpy(ac_rt_info_->download_task.x_ks_cache, this->connection_info_.x_ks_cache.c_str(), CDN_X_KS_CACHE_MAX_LEN);
    strncpy(ac_rt_info_->download_task.resolved_ip, this->connection_info_.ip.c_str(), DATA_SOURCE_IP_MAX_LEN);
    ac_rt_info_->download_task.http_connect_ms = this->connection_info_.connection_used_time_ms;
    ac_rt_info_->download_task.http_first_data_ms = this->connection_info_.http_first_data_ms;
    ac_rt_info_->download_task.http_dns_analyze_ms = this->connection_info_.http_dns_analyze_ms;

    av_dict_free(&format_opts);

    if (ret < 0) {
        return avio_error_to_ac_error(ret);
    }

    return this->opts_.is_live ? std::numeric_limits<int64_t>::max() : this->connection_info_.content_length;
}

int64_t FFUrlHttpDataSource::Read(uint8_t* buf, int64_t offset, int64_t len) {
    if (!this->ctx_) {
        return kResultExceptionSourceNotOpened_0;
    }

    int ret = ffurl_read(this->ctx_, buf + offset, (int)len);

    if (ret == 0) {
        return kResultExceptionHttpDataSourceReadNoData;
    }

    if (ret < 0) {
        return avio_error_to_ac_error(ret);
    }

    this->downloaded_bytes_ += ret;
    this->connection_info_.UpdateDownloadedSize(this->downloaded_bytes_);
    if (this->connection_info_.IsDownloadComplete()) {
        this->connection_info_.stop_reason_ = kDownloadStopReasonFinished;
        this->connection_info_.transfer_consume_ms_ = (int)(kpbase::SystemUtil::GetCPUTime() - this->timestamp_start_download_);
    }

    return ret;
}

AcResultType FFUrlHttpDataSource::Close() {
    if (!this->ctx_) {
        return kResultExceptionSourceNotOpened_0;
    }
    int ret = ffurl_closep(&this->ctx_);
    if (ret < 0) {
        return avio_error_to_ac_error(ret);
    }
    return ret;
}


ConnectionInfo const& FFUrlHttpDataSource::GetConnectionInfo() {
    return this->connection_info_;
}
