#ifdef CONFIG_AEGON

#include "./scope_cronet_http_task.h"
#include "./http_task_header_utils.h"

#include "cache_errors.h"
#include "utility.h"
#include "runloop.h"
#include "cache/cache_util.h"

#include <aegon/aegon.h>

extern "C" {
#include <curl/curl.h>
}

using namespace kuaishou::cache;

#define LOG_PREFIX "[ScopeCronetHttpTask] "

#define READ_BUFFER_SIZE (256 * 1024)

// error code of libcurl aborted, used for compatibility
static const AcResultType kResultUserInterrupt = (kLibcurlErrorBase - CURLE_ABORTED_BY_CALLBACK);

namespace {
// Some preprocessor hacks to define wrappers for callbacks
#define COMMA ,
#define CALLBACK_WRAPPER(NAME, EXTRA_ARGS_DEF, EXTRA_ARGS) \
    void NAME ## _wrapper(Cronet_UrlRequestCallbackPtr callback, Cronet_UrlRequestPtr request, EXTRA_ARGS_DEF) { \
        reinterpret_cast<ScopeCronetHttpTask*>(Cronet_UrlRequestCallback_GetClientContext(callback)) \
        ->NAME(EXTRA_ARGS); \
    }

CALLBACK_WRAPPER(OnRedirectReceived, Cronet_UrlResponseInfoPtr info COMMA Cronet_String new_location_url, info COMMA new_location_url);
CALLBACK_WRAPPER(OnResponseStarted, Cronet_UrlResponseInfoPtr info, info);
CALLBACK_WRAPPER(OnReadComplete, Cronet_UrlResponseInfoPtr info COMMA Cronet_BufferPtr buffer COMMA uint64_t bytes_read, info COMMA buffer COMMA bytes_read);
CALLBACK_WRAPPER(OnSucceeded, Cronet_UrlResponseInfoPtr info, info);
CALLBACK_WRAPPER(OnFailed, Cronet_UrlResponseInfoPtr info COMMA Cronet_ErrorPtr error, info COMMA error);
CALLBACK_WRAPPER(OnCancelled, Cronet_UrlResponseInfoPtr info, info);

#undef CALLBACK_WRAPPER
#undef COMMA

Cronet_ExecutorPtr g_executor = nullptr;
std::mutex g_executor_mtx;

void inline_executor(Cronet_ExecutorPtr self, Cronet_RunnablePtr command) {
    Cronet_Runnable_Run(command);
    Cronet_Runnable_Destroy(command);
}

Cronet_ExecutorPtr get_global_executor() {
    std::lock_guard<std::mutex> lock(g_executor_mtx);
    if (!g_executor) {
        g_executor = Cronet_Executor_CreateWith(&inline_executor);
    }
    return g_executor;
}

static int g_index = 0;

std::unique_ptr<kuaishou::kpbase::Runloop> g_callback_runloop;
std::mutex g_callback_runloop_mtx;

kuaishou::kpbase::Runloop* get_global_callback_runloop() {
    std::lock_guard<std::mutex> lock(g_callback_runloop_mtx);
    if (!g_callback_runloop) {
        g_callback_runloop.reset(new kuaishou::kpbase::Runloop("CronetHttpTaskCallback"));
    }
    return g_callback_runloop.get();
}

}

bool ScopeCronetHttpTask::IsEnabled() {
    return Aegon_GetCronetEngine() != nullptr;
}

ScopeCronetHttpTask::ScopeCronetHttpTask(const DownloadOpts& opts, ScopeTaskListener* listener,
                                         AwesomeCacheRuntimeInfo* ac_rt_info):
    id_(g_index++), opts_(opts), listener_(listener), ac_rt_info_(ac_rt_info),
    progress_helper_(id_, opts, &connection_info_, ac_rt_info) {
    this->cronet_callback_ = Cronet_UrlRequestCallback_CreateWith(
                                 &OnRedirectReceived_wrapper, &OnResponseStarted_wrapper, &OnReadComplete_wrapper,
                                 &OnSucceeded_wrapper, &OnFailed_wrapper, &OnCancelled_wrapper);
    Cronet_UrlRequestCallback_SetClientContext(this->cronet_callback_, this);

    if (ac_rt_info_) {
        ac_rt_info_->download_task.con_timeout_ms = opts_.connect_timeout_ms;
        ac_rt_info_->download_task.read_timeout_ms = opts_.read_timeout_ms;
    }
}

ScopeCronetHttpTask::~ScopeCronetHttpTask() {
    LOG_DEBUG(LOG_PREFIX "[%d] Destruct...", id_);
    Cronet_UrlRequestCallback_Destroy(this->cronet_callback_);
}

int64_t ScopeCronetHttpTask::Open(const DataSpec& spec) {
    this->recv_valid_bytes_ = 0;
    this->server_not_support_range_ = false;
    this->to_skip_bytes_ = 0;
    this->skipped_bytes_ = 0;

    this->spec_ = spec;
    Cronet_EnginePtr engine = Aegon_GetCronetEngine();
    if (!engine) {
        LOG_ERROR(LOG_PREFIX "[%d] Aegon engine not ready", id_);
        return kCronetCreateFailed;
    }
    auto params = Cronet_UrlRequestParams_Create();
    this->cronet_request_ = Cronet_UrlRequest_Create();

    auto add_header = [&, this](std::string const & k, std::string const & v) {
        if (k == "Range" && MockServerRangeNotSupport())
            return;
        LOG_INFO(LOG_PREFIX "[%d] Add header: %s: %s", id_, k.c_str(), v.c_str());
        Cronet_HttpHeaderPtr header = Cronet_HttpHeader_Create();
        Cronet_HttpHeader_name_set(header, k.c_str());
        Cronet_HttpHeader_value_set(header, v.c_str());
        Cronet_UrlRequestParams_request_headers_add(params, header);
        Cronet_HttpHeader_Destroy(header);
    };
    http::SetRequestHeaders(spec, this->opts_, &this->connection_info_, add_header);

    this->connection_info_.download_uuid = CacheUtil::GenerateUUID();
    this->connection_info_.session_uuid = this->opts_.session_uuid.empty() ? "NULL" : this->opts_.session_uuid;
    std::string user_agent = this->opts_.user_agent
                             + "/" + connection_info_.session_uuid
                             + "/" + connection_info_.download_uuid
                             + "/cache";
    add_header("user-agent", user_agent);

    add_header(AEGON_REQUEST_HEADER_SKIP_CERT_VERIFY, "1");
    add_header(AEGON_REQUEST_HEADER_CONNECT_TIMEOUT, std::to_string(this->opts_.connect_timeout_ms));
    add_header(AEGON_REQUEST_HEADER_READ_TIMEOUT, std::to_string(this->opts_.read_timeout_ms));
    if (this->opts_.max_speed_kbps > 0) {
        // Only take effect for aegon version >= 1.0, but no side effect for old aegon versions
        add_header("x-aegon-throttling-kbps", std::to_string(this->opts_.max_speed_kbps));
    }

    this->start_t_ = std::chrono::steady_clock::now();
    progress_helper_.OnStart();

    Cronet_UrlRequest_InitWithParams(this->cronet_request_, engine, spec.uri.c_str(), params,
                                     this->cronet_callback_, get_global_executor());
    auto start_res = Cronet_UrlRequest_Start(this->cronet_request_);
    Cronet_UrlRequestParams_Destroy(params);

    // Only set request to this->cronet_request_ if it's successfully started
    if (start_res == Cronet_RESULT_SUCCESS) {
        return 0;
    } else {
        Cronet_UrlRequest_Destroy(this->cronet_request_);
        this->cronet_request_ = nullptr;
        return kCronetCreateFailed;
    }
}

void ScopeCronetHttpTask::Abort() {
    if (!this->cronet_request_)
        return;
    LOG_INFO(LOG_PREFIX "[%d] Cancelling request", id_);
    Cronet_UrlRequest_Cancel(this->cronet_request_);
}

void ScopeCronetHttpTask::Close() {
    if (!this->cronet_request_)
        return;
    this->Abort();
    this->WaitForTaskFinish();

    Cronet_UrlRequest_Destroy(this->cronet_request_);
    this->cronet_request_ = nullptr;
}

void ScopeCronetHttpTask::WaitForTaskFinish() {
    if (!this->cronet_request_)
        return;
    LOG_INFO(LOG_PREFIX "[%d] Waiting for done...", id_);
    std::unique_lock<std::mutex> lock(this->mtx_);
    this->cond_.wait(lock, [this]() {
        // return Cronet_UrlRequest_IsDone(this->cronet_request_);
        return this->finished_;
    });
    LOG_INFO(LOG_PREFIX "[%d] Waiting for done...done", id_);
}

void ScopeCronetHttpTask::OnRedirectReceived(Cronet_UrlResponseInfoPtr info, Cronet_String new_location_url) {
    LOG_INFO(LOG_PREFIX "[%d] OnRedirectReceived: %s", id_, new_location_url);
    bool internal_redirect = false;
    for (size_t i = 0 ; i < Cronet_UrlResponseInfo_all_headers_list_size(info) ; i += 1) {
        auto header = Cronet_UrlResponseInfo_all_headers_list_at(info, i);
        if (strcmp(Cronet_HttpHeader_name_get(header), "Non-Authoritative-Reason") == 0 &&
            strcmp(Cronet_HttpHeader_value_get(header), "Delegate") == 0) {
            internal_redirect = true;
            break;
        }
    }
    if (!internal_redirect) {
        connection_info_.redirect_count += 1;
        connection_info_.effective_url = new_location_url;
    }
    Cronet_UrlRequest_FollowRedirect(this->cronet_request_);
}

void ScopeCronetHttpTask::OnResponseStarted(Cronet_UrlResponseInfoPtr info) {
    this->connection_info_.http_first_data_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                    std::chrono::steady_clock::now() - this->start_t_).count();
    this->connection_info_.response_code = Cronet_UrlResponseInfo_http_status_code_get(info);

    if (!this->connection_info_.IsResponseCodeSuccess()) {
        LOG_ERROR(LOG_PREFIX "[%d] Bad http response code: %d",
                  id_, connection_info_.response_code);
        if (this->listener_) {
            this->listener_->OnDownloadComplete(kHttpInvalidResponseCodeBase - connection_info_.response_code, kDownloadStopReasonFailed);
        }
        return;
    }

    int header_i = 0;
    auto get_next_header_fn = [&]() {
        if (header_i == Cronet_UrlResponseInfo_all_headers_list_size(info))
            return std::pair<std::string, std::string>();
        Cronet_HttpHeaderPtr header = Cronet_UrlResponseInfo_all_headers_list_at(info, header_i++);
        std::string name = Cronet_HttpHeader_name_get(header);
        std::string value = Cronet_HttpHeader_value_get(header);

        LOG_DEBUG(LOG_PREFIX "[%d] Received header: %s: %s", id_, name.c_str(), value.c_str());
        if (name == AEGON_RESPONSE_HEADER_REMOTE_IP)
            connection_info_.ip = value;
        if (name == AEGON_RESPONSE_HEADER_DNS_COST_MS)
            connection_info_.http_dns_analyze_ms = atoi(value.c_str());
        if (name == AEGON_RESPONSE_HEADER_CONNECT_COST_MS)
            connection_info_.connection_used_time_ms = atoi(value.c_str());

        return std::make_pair(name, value);
    };

    int64_t parse_ret = http::ParseResponseHeaders(this->spec_, &this->connection_info_,
                                                   &this->server_not_support_range_,
                                                   &this->to_skip_bytes_,
                                                   get_next_header_fn);
    if (parse_ret < 0) {
        LOG_ERROR(LOG_PREFIX "[%d] Parse response header error", id_);
        if (this->listener_) {
            this->listener_->OnDownloadComplete(kCronetHttpResponseHeaderInvalid, kDownloadStopReasonFailed);
        }
        return;
    }

    if (ac_rt_info_) {
        ac_rt_info_->download_task.http_response_code = connection_info_.response_code;
        ac_rt_info_->download_task.http_connect_ms = connection_info_.connection_used_time_ms;
        ac_rt_info_->download_task.http_dns_analyze_ms = connection_info_.http_dns_analyze_ms;
        ac_rt_info_->download_task.http_first_data_ms = connection_info_.http_first_data_ms;
        snprintf(ac_rt_info_->download_task.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
                 connection_info_.ip.c_str());
        strncpy(ac_rt_info_->download_task.http_version,
                Cronet_UrlResponseInfo_negotiated_protocol_get(info),
                HTTP_VERSION_MAX_LEN);
    }

    LOG_INFO(LOG_PREFIX "[%d] OnResponseStarted: httpcode=%d", id_, connection_info_.response_code);
    if (this->listener_) {
        this->listener_->OnConnectionInfoParsed(this->connection_info_);
    }

    Cronet_BufferPtr buffer = Cronet_Buffer_Create();
    Cronet_Buffer_InitWithAlloc(buffer, READ_BUFFER_SIZE);
    Cronet_UrlRequest_Read(this->cronet_request_, buffer);
}

void ScopeCronetHttpTask::OnReadComplete(Cronet_UrlResponseInfoPtr info, Cronet_BufferPtr buffer, uint64_t bytes_read) {
    if (AwesomeCacheInterruptCB_is_interrupted(&this->opts_.interrupt_cb)) {
        LOG_INFO(LOG_PREFIX "[%d] User interruptted, cancel and stop reading", id_);
        Cronet_UrlRequest_Cancel(this->cronet_request_);
        return;
    }

    this->progress_helper_.OnProgress(Cronet_UrlResponseInfo_received_byte_count_get(info));

    uint8_t* data = (uint8_t*)Cronet_Buffer_GetData(buffer);
    bool should_cancel = false;

    if (this->server_not_support_range_) {
        int64_t remain_to_skip_bytes = this->to_skip_bytes_ - this->skipped_bytes_;

        if (remain_to_skip_bytes >= bytes_read) {
            // skip all bytes
            this->skipped_bytes_ += bytes_read;
            bytes_read = 0;
        } else {
            this->skipped_bytes_ += remain_to_skip_bytes;
            auto valid_bytes = static_cast<int64_t>(bytes_read - (remain_to_skip_bytes));
            if (spec_.length > 0 && valid_bytes > spec_.length - recv_valid_bytes_) {
                // 不支持range请求的case，需要在满足了spec里的长度要求后就停止下载
                data += remain_to_skip_bytes;
                bytes_read = spec_.length - recv_valid_bytes_;
                should_cancel = true;
            } else {
                data += remain_to_skip_bytes;
                bytes_read = valid_bytes;
            }
        }
    }

    this->recv_valid_bytes_ += bytes_read;
    if (this->listener_ && bytes_read > 0) {
        this->listener_->OnReceiveData(data, bytes_read);
    }

    if (should_cancel) {
        LOG_INFO(LOG_PREFIX "[%d] Server does not support Range, read enough, cancel and stop reading", id_);
        Cronet_UrlRequest_Cancel(this->cronet_request_);
    } else {
        Cronet_UrlRequest_Read(this->cronet_request_, buffer);
    }
}

void ScopeCronetHttpTask::OnFinished(int32_t error, int32_t stop_reason) {
    this->progress_helper_.OnFinish();
    if (ac_rt_info_) {
        ac_rt_info_->download_task.download_total_cost_ms = connection_info_.transfer_consume_ms;
        ac_rt_info_->download_task.downloaded_bytes = connection_info_.downloaded_bytes_from_curl;
        ac_rt_info_->download_task.recv_valid_bytes = recv_valid_bytes_;
        strncpy(ac_rt_info_->download_task.kwaisign, this->connection_info_.sign.c_str(), CDN_KWAI_SIGN_MAX_LEN);
        strncpy(ac_rt_info_->download_task.x_ks_cache, this->connection_info_.x_ks_cache.c_str(), CDN_X_KS_CACHE_MAX_LEN);
        ac_rt_info_->cache_v2_info.skip_scope_cnt += server_not_support_range_ ? 1 : 0;
        ac_rt_info_->cache_v2_info.skip_total_bytes += skipped_bytes_;
    }

    // listener->OnDownloadComplete involves file operation, which may block a long time (several hundreds milliseconds)
    // Also, this function is called in cronet network thread (we use inline_executor), which may never be blocked
    // So, post this callback to a seperate (global) runloop
    // Use a global runloop to prevent creating seperate thread for each task
    get_global_callback_runloop()->Post([ = ]() {
        if (this->listener_) {
            this->listener_->OnDownloadComplete(error, stop_reason);
        }
        std::lock_guard<std::mutex> lock(this->mtx_);
        this->finished_ = true;
        this->cond_.notify_all();
    });
}

void ScopeCronetHttpTask::OnSucceeded(Cronet_UrlResponseInfoPtr info) {
    LOG_INFO(LOG_PREFIX "[%d] OnSucceeded", id_);
    this->OnFinished(0, kDownloadStopReasonFinished);
}

void ScopeCronetHttpTask::OnFailed(Cronet_UrlResponseInfoPtr info, Cronet_ErrorPtr error) {
    bool interrupted = AwesomeCacheInterruptCB_is_interrupted(&this->opts_.interrupt_cb);
    LOG_INFO(LOG_PREFIX "[%d] OnFailed (interrupted? %d)", id_, interrupted);
    if (interrupted)
        this->OnFinished(kResultUserInterrupt, kDownloadStopReasonCancelled);
    else
        this->OnFinished(
            kCronetInternalErrorBase + Cronet_Error_internal_error_code_get(error),
            kDownloadStopReasonFailed);
}

void ScopeCronetHttpTask::OnCancelled(Cronet_UrlResponseInfoPtr info) {
    LOG_INFO(LOG_PREFIX "[%d] OnCancelled", id_);
    this->OnFinished(kResultUserInterrupt, kDownloadStopReasonCancelled);
}

#endif
