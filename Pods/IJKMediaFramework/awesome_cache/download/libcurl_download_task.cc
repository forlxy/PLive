#include <algorithm>
#include <include/awesome_cache_runtime_info_c.h>
#include "libcurl_download_task.h"
#include "utility.h"
#include "ac_log.h"
#include "cache_session_listener.h"
#include <json/json.h>
#include <cache/cache_util.h>
#include "ac_utils.h"
#include <regex>

using json = nlohmann::json;
#include "dcc_algorithm_c.h"
#include "libcurl_connection_reuse_manager.h"

namespace kuaishou {
namespace cache {
namespace {
static const int kClosed = 1;
static const int kConnected = 2;

static const int kVerbose = false;

static const int MAX_HTTP_X_KS_COUNT = 10;

}


int ProgressCallback(LibcurlDownloadTask* task,
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t ultotal, curl_off_t ulnow);
size_t HeaderCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTask* task);
size_t WriteCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTask* task);

int SockOptCallback(void* clientp,
                    curl_socket_t curlfd,
                    curlsocktype purpose);

LibcurlDownloadTask::LibcurlDownloadTask(const DownloadOpts& opts,
                                         AwesomeCacheRuntimeInfo* ac_rt_info) :
    options_(opts),
    curl_(nullptr),
    curl_info_collected_(false),
    last_progress_time_ms_(0),
    last_progress_bytes_(0),
    feed_data_consume_ms_(0),
    task_total_consume_ms_(0),
    transfer_over_recorded_(false),
    state_(kClosed),
    abort_(false),
    last_dlnow_(0),
    last_progress_callback_ts_ms_(0),
    http_header_parsed_(false),
    connection_info_(DownloadTask::id()),
    ac_rt_info_(ac_rt_info) {
    SetPriority(opts.priority);
    interrupt_callback_ = opts.interrupt_cb;
    context_id_ = opts.context_id;
    buffer_size_ = std::min(std::max(kMinCurlBufferSizeKb, opts.curl_buffer_size_kb),
                            kMaxCurlBufferSizeKb) * KB;
    input_stream_buffer_max_used_bytes_ = 0;

    if (ac_rt_info_ != nullptr) {
        AwesomeCacheRuntimeInfo_download_task_init(ac_rt_info_);
        ac_rt_info_->download_task.curl_buffer_size_kb = buffer_size_ / 1024;
        ac_rt_info_->download_task.con_timeout_ms = options_.connect_timeout_ms;
        ac_rt_info_->download_task.read_timeout_ms = options_.read_timeout_ms;
    }
}

LibcurlDownloadTask::~LibcurlDownloadTask() {
    Close();
    curl_ = nullptr;
}

void* run_thread(void* opaque) {
    LibcurlDownloadTask* task = static_cast<LibcurlDownloadTask*>(opaque);
    task->Run();
    return NULL;
}

ConnectionInfo LibcurlDownloadTask::MakeConnection(const DataSpec& spec) {
    speed_cal_ = std::make_shared<SpeedCalculator>();

    connection_info_.download_uuid = CacheUtil::GenerateUUID();
    connection_info_.session_uuid = options_.session_uuid.empty() ? "NULL" : options_.session_uuid;

    user_agent_ = options_.user_agent
                  + "/" + connection_info_.session_uuid
                  + "/" + connection_info_.download_uuid
                  + "/cache";

    LOG_INFO("[%d] [LibcurlDownloadTask::MakeConnection] id: %d, start, spec.position:%lld, spec.length:%lld, buffer_size:%dKB\n",
             context_id_, id(), spec.position, spec.length, buffer_size_ / KB);

    spec_ = spec;
    if (input_stream_) {
        input_stream_->Close();
    }
    input_stream_buffer_max_used_bytes_ = 0;
    input_stream_ = make_shared<BlockingInputStream>(buffer_size_);

    int ret = pthread_create(&thread_id_, NULL, run_thread, this);
    if (ret != 0) {
        LOG_ERROR("[%d] LibcurlDownloadTask::MakeConnection, pthread_create fail", context_id_);
        thread_joined = true;
    } else {
        thread_joined = false;
    }
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->datasource_index++;
    }
    // wait for connection open signal.
    connection_opened_.Wait();
    return connection_info_;
}

void LibcurlDownloadTask::Pause() {
}

void LibcurlDownloadTask::Resume() {
}

void LibcurlDownloadTask::Close() {
    input_stream_->Close();
    abort_ = true;
    if (!thread_joined) {
        int join_ret = pthread_join(thread_id_, NULL);
        if (join_ret != 0) {
            LOG_ERROR("[%d] LibcurlDownloadTask::Close, pthread_join fail", context_id_);
        }
        thread_joined = true;
    }
}

void LibcurlDownloadTask::ParseHeader() {
    std::string header = http_header_;
    LOG_DEBUG("[%d] [LibcurlDownloadTask::ParseHeader] id:%d, http_header:%s", context_id_, id(), http_header_.c_str());
    // todo, if redirects, there will be multiple http headers, we only care about the last one.

    std::string return_str = "";
    int return_str_len = 0;

    if (header.find("\r\n") != -1) {
        return_str = "\r\n";
        return_str_len = 2;
    } else if (header.find("\n") != -1) {
        return_str = "\n";
        return_str_len = 1;
    } else if (header.find("\r") != -1) {
        return_str = "\r";
        return_str_len = 1;
    } else {
        LOG_ERROR_DETAIL("[%d] [LibcurlDownloadTask::ParseHeader] id:%d, fail, have not found any return_str \n", context_id_, id());
        return;
    }

    int http_x_ks_cnt = 0;
    int http_x_ks_str_len = strlen("x-ks-");
    for (;;) {
        size_t position = header.find(return_str);
        if (position == -1) {
            break;
        }

        std::string sub_header = header.substr(0, position);
        size_t colon_pos = sub_header.find(":");
        if (colon_pos != -1) {
            std::string key = kpbase::StringUtil::Trim(sub_header.substr(0, colon_pos));
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            std::string value = kpbase::StringUtil::Trim(sub_header.substr(colon_pos + 1));
            if (!key.compare(0, http_x_ks_str_len, "x-ks-")) {
                http_x_ks_cnt++;
                if (http_x_ks_cnt < MAX_HTTP_X_KS_COUNT) {
                    std::string x_ks_cache_value = key + ": " + value;
                    http_x_ks_headers_.push_back(x_ks_cache_value);
                }
            } else {
                http_headers_[key] = value;
            }
        }
        header = header.substr(position + return_str_len);
        http_header_parsed_ = true;
    }
}

void LibcurlDownloadTask::GetHttpXKsJsonString() {
    int size = http_x_ks_headers_.size();

    if (size == 0) {
        connection_info_.x_ks_cache = "{}";
        return;
    }

    json j;
    for (int i = 0; i < size; i++) {
        j.push_back(http_x_ks_headers_[i]);
    }
    connection_info_.x_ks_cache = j.dump();
}

std::shared_ptr<InputStream> LibcurlDownloadTask::GetInputStream() {
    return input_stream_;
}

const ConnectionInfo& LibcurlDownloadTask::GetConnectionInfo() {
    return connection_info_;
}

bool LibcurlDownloadTask::IsInterrupted() {
    return AwesomeCacheInterruptCB_is_interrupted(&interrupt_callback_);
}

void LibcurlDownloadTask:: OnTransferOver() {
    if (transfer_over_recorded_) {
        return;
    }
    int64_t end = kpbase::SystemUtil::GetCPUTime();
    task_total_consume_ms_ = (int)(end - task_start_download_ts_ms_);
    transfer_over_recorded_ = true;

    // 这个数据很重要，最终要通过onDownloadStop返给cdn
    connection_info_.transfer_consume_ms_ = task_total_consume_ms_ - feed_data_consume_ms_;

    if (ac_rt_info_ != nullptr) {
        AwesomeCacheRuntimeInfo_download_task_end(ac_rt_info_);
        ac_rt_info_->download_task.download_total_cost_ms = task_total_consume_ms_;
        ac_rt_info_->http_ds.task_downloaded_bytes = ac_rt_info_->http_ds.task_downloaded_bytes + connection_info_.downloaded_bytes_;
    }
    if (speed_cal_->IsMarkValid()) {
        if (kVerbose) {
            LOG_DEBUG("[%d] [dccAlg]before DccAlgorithm_update_speed_mark, speed_cal, sample_cnt:%d",
                      context_id_, speed_cal_->GetSampleCnt());
        }
        DccAlgorithm_update_speed_mark(speed_cal_->GetMarkSpeedKbps());
    }
}

void LibcurlDownloadTask::UpdateDownloadedSizeFromCurl() {
    curl_off_t download_size = 0;
    curl_easy_getinfo(curl_, CURLINFO_SIZE_DOWNLOAD_T, &download_size);
    connection_info_.UpdateDownloadedSize(download_size);
    UpdateSpeedCalculator();
    UpdateDownloadBytes();
    int64_t current_time = AcUtils::GetCurrentTime();
    if (current_time - last_log_time_ > KDownload_Log_Interval_Ms &&
        download_size - last_log_size_ > KDownload_Log_Bytes_Threshold) {
        last_log_size_ = download_size;
        last_log_time_ = current_time;
        if (kVerbose) {
            LOG_INFO("[%d][DownloadRecord] id: %d, time:%lld, task_downloadsize:%lld, "
                     "total_downloadsize:%lld",
                     context_id_, id(), current_time, download_size,
                     (ac_rt_info_ != nullptr) ? ac_rt_info_->http_ds.download_bytes : 0);
        }
    }
}

void LibcurlDownloadTask::UpdateSpeedCalculator() {
    speed_cal_->Update(connection_info_.GetDownloadedBytes());
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->download_task.speed_cal_current_speed_index++;
        ac_rt_info_->download_task.speed_cal_current_speed_kbps = speed_cal_->GetCurrentSpeedKbps();
        ac_rt_info_->download_task.speed_cal_avg_speed_kbps  = speed_cal_->GetAvgSpeedKbps();
        ac_rt_info_->download_task.speed_cal_mark_speed_kbps = speed_cal_->GetMarkSpeedKbps();
    }
}

void LibcurlDownloadTask::UpdateDownloadBytes() {
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->http_ds.download_bytes = ac_rt_info_->http_ds.task_downloaded_bytes + connection_info_.GetDownloadedBytes();
    }
}

void LibcurlDownloadTask::OnAfterFeedData(int32_t buffer_cur_used_bytes) {
    if (input_stream_buffer_max_used_bytes_ < buffer_cur_used_bytes) {
        input_stream_buffer_max_used_bytes_ = buffer_cur_used_bytes;
        if (ac_rt_info_ != nullptr) {
            ac_rt_info_->download_task.curl_buffer_max_used_kb = input_stream_buffer_max_used_bytes_ / 1024;
        }
    }
}

void LibcurlDownloadTask::LimitCurlSpeed() {
    if (curl_) {
        curl_off_t max_speed = 1;
        curl_easy_setopt(curl_, CURLOPT_MAX_RECV_SPEED_LARGE, max_speed);
    }
}

void LibcurlDownloadTask::Run() {
    curl_ = curl_easy_init();
    LibcurlConnectionReuseManager::Setup(curl_, this->options_);

    AcUtils::SetThreadName("LibcurlDownloadTask");

    LOG_INFO("[%d] [LibcurlDownloadTask::Run] id: %d, curl_(:%p),spec_.position:%lld, start request: uri %s",
             context_id_, id(), curl_, spec_.position, spec_.uri.c_str());
    if (curl_) {
        connection_info_.uri = spec_.uri;
        // parse http headers.
        struct curl_slist* header_list = NULL;
        if (!options_.headers.empty()) {
            auto headers = kpbase::StringUtil::Split(options_.headers, "\r\n");
            for (std::string& header : headers) {
                auto trimmed = kpbase::StringUtil::Trim(header);
                if (!trimmed.empty()) {
                    header_list = curl_slist_append(header_list, trimmed.c_str());
                }
                auto key_value_vec = kpbase::StringUtil::Split(header, ": ");
                if (key_value_vec.size() == 2) {
                    auto key = kpbase::StringUtil::Trim(key_value_vec[0]);
                    auto value = kpbase::StringUtil::Trim(key_value_vec[1]);
                    if (key == "Host" || key == "host") {
                        connection_info_.host = value;
                    }
                }
            }
        }
        // parse and replace ip in https url with domain
        // 问题背景：libcurl进行https ip访问时，aws cdn返回ssl握手失败，应该是服务端没有从SNI拿到域名匹配到证书拒绝了连接
        std::string url = spec_.uri;
        struct curl_slist* host_list = nullptr;
        if (IsHttpsScheme(spec_.uri)) {
            std::string ip;
            // 通过设置CURLOPT_RESOLVE（HOST:PORT:ADDRESS），可以指定访问ip实现https ip直连
            if (ParseAndReplaceIpWithDomain(url, ip)) {
                std::string host_ip_str = connection_info_.host + ":443:" + ip;
                auto trimmed = kpbase::StringUtil::Trim(host_ip_str);
                host_list = curl_slist_append(host_list, trimmed.c_str());
                curl_easy_setopt(curl_, CURLOPT_RESOLVE, host_list);
            }
        }
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, user_agent_.c_str());
        curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 50L);
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
        std::regex reg("Accept-Encoding:[\t ]*(.*)\r\n");
        smatch m;
        regex_search(options_.headers, m, reg);
        if (m[1].matched) {
            curl_easy_setopt(curl_, CURLOPT_ACCEPT_ENCODING, m[1].str().c_str());
        }

        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
        // Option for HTTP Connect Timeout   // 注意，这个接口的的单位是秒
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, options_.connect_timeout_ms / 1000);
        // http://www.cnblogs.com/edgeyang/articles/3722035.html
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);

        if (options_.http_proxy_address.length() > 0) {
            curl_easy_setopt(curl_, CURLOPT_PROXY, options_.http_proxy_address.c_str());
            curl_easy_setopt(curl_, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
            LOG_INFO("[%d] [LibcurlDownloadTask::Run] id:%d, use http_proxy_address:%s",
                     context_id_, id(), options_.http_proxy_address.c_str());
        }

        LOG_INFO("[%d] [LibcurlDownloadTask::Run] id:%d, connect_timeout_ms :%d ms, read_timeout_ms:%dms",
                 context_id_, id(), options_.connect_timeout_ms, options_.read_timeout_ms);
        // add http headers.
        if (spec_.position != 0 || (spec_.length != kLengthUnset && spec_.length > 0)) {
            connection_info_.range_request_start = spec_.position;
            std::string range_request = "Range: bytes=" + kpbase::StringUtil::Int2Str(spec_.position) + "-";
            if (spec_.length != kLengthUnset && spec_.length > 0) {
                int64_t range_end = spec_.position + spec_.length - 1;
                range_request += kpbase::StringUtil::Int2Str(range_end);
                connection_info_.range_request_end = range_end;
            }
            header_list = curl_slist_append(header_list, range_request.c_str());
        }
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);

        if (ac_rt_info_ != nullptr) {
            AwesomeCacheRuntimeInfo_download_task_set_config_user_agent(ac_rt_info_, user_agent_.c_str());
        }

        curl_easy_setopt(curl_, CURLOPT_SOCKOPTFUNCTION, SockOptCallback);
        curl_easy_setopt(curl_, CURLOPT_SOCKOPTDATA, this);

        // progress callback.
        curl_easy_setopt(curl_, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
        curl_easy_setopt(curl_, CURLOPT_XFERINFODATA, this);
        // header write callback
        curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl_, CURLOPT_HEADERDATA, this);
        // write callback.
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);

        if (options_.max_speed_kbps > 0) {
            curl_off_t max_speed = options_.max_speed_kbps * 1024 / 8;
            curl_easy_setopt(curl_, CURLOPT_MAX_RECV_SPEED_LARGE, max_speed);//bytes/s
        }

        curl_info_collected_ = false;
        task_start_download_ts_ms_ = kpbase::SystemUtil::GetCPUTime();

        if (ac_rt_info_ != nullptr) {
            AwesomeCacheRuntimeInfo_download_task_start(ac_rt_info_);
            ac_rt_info_->cache_ds.is_reading_file_data_source = false;
        }
#if defined(__ANDROID__)
        tcp_climing_.Init(buffer_size_, task_start_download_ts_ms_);
#endif
        int curl_ret = curl_easy_perform(curl_);
        if (ac_rt_info_ != nullptr) {
            ac_rt_info_->cache_ds.is_reading_file_data_source = true;
        }
#if defined(__ANDROID__)
        connection_info_.tcp_climbing_info = tcp_climing_.GetTcpClimbingInfoString();
#endif
        UpdateDownloadedSizeFromCurl();
        OnTransferOver();
        CollectCurlInfoOnce();

        long os_errno;
        curl_easy_getinfo(curl_, CURLINFO_OS_ERRNO, &os_errno);
        SetOsErrno(os_errno);

        if (options_.is_live && curl_ret == CURLE_GOT_NOTHING) {
            // for live stream, GOT_NOTHING simply means EOF, not error
            SetStopReason(kDownloadStopReasonFinished);
        } else if (curl_ret != CURLE_OK) {
            // 这快StopReason的逻辑谨慎修改，因为涉及到app层来决定是否需要切换cdn重试，该逻辑会影响卡顿数据
            if (curl_ret == CURLE_OPERATION_TIMEDOUT) {
                // timeout 单独作为一个reason
                SetStopReason(kDownloadStopReasonTimeout);
            } else {
                if (connection_info_.stop_reason_ != kDownloadStopReasonCancelled
                    && connection_info_.stop_reason_ != kDownloadStopReasonFinished) {
                    SetStopReason(kDownloadStopReasonFailed);
                }
            }

            // 如果之前 connection_info_.error_code已经被赋值过了，就不再次赋值了
            if (connection_info_.error_code == 0) {
                int error_code = (curl_ret > 0 ? -curl_ret : curl_ret) + kLibcurlErrorBase;
                SetErrorCode(error_code);
                if (connection_info_.stop_reason_ != kDownloadStopReasonCancelled
                    && connection_info_.stop_reason_ != kDownloadStopReasonFinished) {
                    SetStopReason(kDownloadStopReasonFailed);
                }
            }
        } else if (!connection_info_.IsResponseCodeSuccess()) {
            SetErrorCode(kHttpInvalidResponseCodeBase - connection_info_.response_code);
            if (connection_info_.stop_reason_ != kDownloadStopReasonCancelled
                && connection_info_.stop_reason_ != kDownloadStopReasonFinished) {
                SetStopReason(kDownloadStopReasonFailed);
            }
        } else {
            if (connection_info_.stop_reason_ == kDownloadStopReasonUnknown) {
                SetStopReason(kDownloadStopReasonFinished);
            }
        }

        ConnectionInfo& con_info = connection_info_;

        LOG_INFO("[%d] [LibcurlDownloadTask::Run] id: %d, after curl_easy_perform, ip:%s,  uri:%s \n",
                 context_id_, id(), con_info.ip.c_str(), spec_.uri.c_str());
        LOG_INFO("[%d] [LibcurlDownloadTask::Run] id: %d, error_code：%d, stop_reason:%s, response_code:%ld \n",
                 context_id_, id(), con_info.error_code, CacheSessionListener::DownloadStopReasonToString(con_info.stop_reason_), con_info.response_code);
        LOG_INFO("[%d] [LibcurlDownloadTask::Run] id: %d, total_consume_ms_:%dms, transfer_consume_ms_:%d, dl_speed:%d \n",
                 context_id_, id(), task_total_consume_ms_, con_info.transfer_consume_ms_, con_info.GetAvgDownloadSpeed());

        // 下面几条日志是分析cdn上报用的，勿轻易改格式
        LOG_INFO("[%d] [LibcurlDownloadTask::Run] id: %d, spec_.position:%lld, content_length:%lld, downloaed:%lld, un_downloaed:%lld",
                 context_id_, id(),
                 spec_.position,
                 con_info.GetContentLength(),
                 con_info.GetDownloadedBytes(),
                 con_info.GetUnDownloaedBytes());

        curl_slist_free_all(header_list);
        curl_slist_free_all(host_list);
    } else {
        LOG_ERROR_DETAIL("[%d] [LibcurlDownloadTask::Run] id:%d, curl_ = NULL, curl_easy_init fail \n", context_id_, id());
        SetErrorCodeAndStopReason(kResultExceptionHttpDataSourceCurlInitFail, kDownloadStopReasonFailed);
    }

    // always clean up curl.
    LibcurlConnectionReuseManager::Teardown(curl_);
    curl_easy_cleanup(curl_);
    // Notify input stream end of stream.
    input_stream_->EndOfStream(connection_info_.error_code);

    if (state_ == kClosed) {
        LOG_DEBUG("[%d] [LibcurlDownloadTask::Run] id:%d connection never opened", context_id_, id());
        // for live stream, early disconnection means EOF
        connection_info_.content_length = options_.is_live ? 0 : kLengthUnset;

        connection_opened_.Signal();
    }

    state_ = kClosed;
    connection_info_.connection_closed = true;
    LOG_INFO("[%d] [LibcurlDownloadTask::Run] id: %d COMPLETE", context_id_,  id());
}

size_t HeaderCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTask* task) {
//  LOG_DEBUG("[LibcurlDownloadTask] id:%d, HeaderCallback,size:%lu, nitems：%lu \n", task->id(), size, nitems);
    task->http_header_.append(buffer, size * nitems);

    return size * nitems;
}

size_t WriteCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTask* task) {

//  LOG_DEBUG("[LibcurlDownloadTask] WriteCallback,size:%ld , task->downloaded_bytes_:%llu \n",
//            (long)(size * nitems), task->connection_info_.downloaded_bytes_);
    // Handle make connection open.
    if (task->state_ == kClosed) {
        task->state_ = kConnected;
        // Parse Header.
        task->ParseHeader();

        task->CollectCurlInfoOnce();

        // responce_code 判断错误
        if (!task->connection_info_.IsResponseCodeSuccess()) {
            task->SetErrorCodeAndStopReason(kHttpInvalidResponseCodeBase - task->connection_info_.response_code, kDownloadStopReasonFailed);
        }

        if (task->http_headers_.find("kwaisign") != task->http_headers_.end()) {
            task->connection_info_.sign = task->http_headers_["kwaisign"];
        } else {
            task->connection_info_.sign = "no value";
        }

        task->GetHttpXKsJsonString();

        snprintf(task->ac_rt_info_->download_task.kwaisign, CDN_KWAI_SIGN_MAX_LEN, "%s",
                 task->connection_info_.sign.c_str());
        snprintf(task->ac_rt_info_->download_task.x_ks_cache, CDN_X_KS_CACHE_MAX_LEN, "%s",
                 task->connection_info_.x_ks_cache.c_str());

        if (task->spec_.position > 0) {
            std::map<std::string, std::string>::iterator it = task->http_headers_.find("content-range");
            if (it == task->http_headers_.end()) {
                task->connection_info_.need_drop_bytes_ = task->spec_.position;
                LOG_DEBUG("[%d] need drop data bytes: %d", task->context_id_, task->connection_info_.need_drop_bytes_);
            } else {
                std::string content_range = it->second;
                std::regex reg1("bytes (\\d+)-(\\d+)/(\\d+)");
                smatch m;
                regex_match(content_range, m, reg1);
                task->connection_info_.range_response_start = m[1].matched ? kpbase::StringUtil::Str2Int(m[1].str()).Value() : -1;
                task->connection_info_.range_response_end = m[2].matched ? kpbase::StringUtil::Str2Int(m[2].str()).Value() : -1;
                task->connection_info_.file_length = m[3].matched ? kpbase::StringUtil::Str2Int(m[3].str()).Value() : -1;
            }
        }

        if (task->http_headers_.find("content-encoding") != task->http_headers_.end()) {
            if (kpbase::StringUtil::Trim(task->http_headers_["content-encoding"]) == "gzip") {
                task->connection_info_.is_gzip = true;
            }
        }

        int64_t content_length = kLengthUnset;
        if (task->options_.is_live) {
            LOG_DEBUG("[%d] set content_length to MAX for live stream, ignore content-length", task->context_id_);
            content_length = std::numeric_limits<int64_t>::max();
        } else if (task->http_headers_.find("content-length") != task->http_headers_.end()) {
            auto maybe_length = kpbase::StringUtil::Str2Int(task->http_headers_["content-length"]);
            if (!maybe_length.IsNull()) {
                content_length = maybe_length.Value();
                if (content_length <= task->connection_info_.need_drop_bytes_) {
                    LOG_ERROR_DETAIL("[%d] content-length Invalid , error_code = "
                                     "kDownloadStopReasonContentLengthInvalid, curl content_length:%lld, drop data length: %d \n",
                                     task->context_id_, content_length, task->connection_info_.need_drop_bytes_);
                    if (content_length <= 0) {
                        task->SetErrorCode(kResultExceptionHttpDataSourceInvalidContentLength);
                    } else {
                        task->SetErrorCode(kResultExceptionHttpDataSourceInvalidContentLengthForDrop);
                    }
                    task->SetStopReason(kDownloadStopReasonContentLengthInvalid);
                }
            } else {
                LOG_ERROR_DETAIL("[%d] content-length not found , error_code = kDownloadStopReasonNoContentLength", task->context_id_);
                task->SetErrorCodeAndStopReason(kResultExceptionHttpDataSourceNoContentLength, kDownloadStopReasonNoContentLength);
            }
        } else {
            if (task->options_.allow_content_length_unset) {
                content_length = kLengthUnset;
            } else {
                LOG_ERROR_DETAIL("[%d] content-length not found , error_code = kDownloadStopReasonNoContentLength", task->context_id_);
                task->SetErrorCodeAndStopReason(kResultExceptionHttpDataSourceNoContentLength, kDownloadStopReasonNoContentLength);
            }
        }

        task->connection_info_.content_length = content_length == kLengthUnset ? kLengthUnset : (content_length - task->connection_info_.need_drop_bytes_);
        // notify connected.
        task->connection_opened_.Signal();
//    task->NotifyListeners([ = ](DownloadTaskListener * listener) {
//      listener->OnConnectionOpen(task, task->spec_.position, task->connection_info_);
//    });


        if (task->connection_info_.error_code != 0) {
            LOG_ERROR_DETAIL("[%d] [LibcurlDownloadTask] id:%d, WriteCallback, error_code = %d, set abort_ = true;", task->context_id_,
                             task->id(), task->connection_info_.error_code);
            task->UpdateDownloadedSizeFromCurl();
            task->OnTransferOver();
            task->abort_ = true;
            return CURL_WRITEFUNC_PAUSE;
        }
    }

    int32_t len = (int32_t)(size * nitems - task->connection_info_.need_drop_bytes_);
    if (len > 0) {
        // 这里必须提前自增，因为外部拿到数据的时候，会直接参考这个数据
        task->connection_info_.downloaded_bytes_  += len;
        task->UpdateDownloadedSizeFromCurl();
        //当使用gzip压缩时候，content_length != file_length,不能根据content_length 判断下载完成
        bool download_complete = task->connection_info_.IsDownloadComplete() && !task->connection_info_.is_gzip;
        if (download_complete) {
            task->SetStopReason(kDownloadStopReasonFinished);
            task->OnTransferOver();
        }

        int64_t feed_start = kpbase::SystemUtil::GetCPUTime();
        int32_t input_stream_used_bytes = 0;
        task->input_stream_->FeedDataSync((uint8_t*) buffer + task->connection_info_.need_drop_bytes_,
                                          len, input_stream_used_bytes);
        task->OnAfterFeedData(input_stream_used_bytes);
        if (download_complete) {
            task->input_stream_->EndOfStream(0);
        }
        int64_t feed_end = kpbase::SystemUtil::GetCPUTime();
        int feed_cost_ms = (int)(feed_end - feed_start);
        task->feed_data_consume_ms_ += feed_cost_ms;

        if (feed_cost_ms > 50 && !task->speed_cal_->IsStoped()) {
            LOG_INFO("[%d] [dccAlg][SpeedCalculator::Update], feed_cost_ms = %d", feed_cost_ms);
            task->speed_cal_->Stop();
        }

        if (kVerbose
            && !task->speed_cal_->IsStoped()
            && task->speed_cal_->GetSampleCnt() <= 5) {
            LOG_DEBUG("[%d] [_speedChart][dccAlg][AfterFeedData] buf_used:%d, buf_max_used:%d，feed_cost_ms：%d ms, "
                      "total_dl_kb:%lld,sample_cnt:%d, speed-> current:%d, avg:%d, mark:%d \n",
                      task->context_id_,
                      input_stream_used_bytes / 1024,
                      task->input_stream_buffer_max_used_bytes_ / 1024,
                      task->feed_data_consume_ms_,
                      task->connection_info_.GetDownloadedBytes() / 1024,
                      task->speed_cal_->GetSampleCnt(),
                      task->speed_cal_->GetCurrentSpeedKbps() / 8,
                      task->speed_cal_->GetAvgSpeedKbps() / 8,
                      task->speed_cal_->GetMarkSpeedKbps() / 8);
        }

        task->connection_info_.droped_bytes_ += task->connection_info_.need_drop_bytes_;
        task->connection_info_.need_drop_bytes_ = 0;

        task->ac_rt_info_->download_task.feed_data_consume_ms_ = task->feed_data_consume_ms_;
        task->ac_rt_info_->download_task.download_total_drop_bytes = task->connection_info_.droped_bytes_;
    } else {
        task->connection_info_.need_drop_bytes_ -= size * nitems;
        task->connection_info_.droped_bytes_ +=  size * nitems;
    }

    return size * nitems;
}

int ProgressCallback(LibcurlDownloadTask* task,
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t ultotal, curl_off_t ulnow) {
    // handle abort or interrupted

    bool interrupted = task->IsInterrupted();
    if (task->abort_ || interrupted) {
        LOG_INFO("[%d] [LibcurlDownloadTask::ProgressCallback] id:%d, task->abort_:%d, task->IsInterrupted() = %d,"
                 "error_code_:%d, stop_reason:%d \n",
                 task->context_id_,
                 task->id(), task->abort_, interrupted, task->connection_info_.error_code, task->connection_info_.stop_reason_);

        if (task->connection_info_.error_code == 0 && task->connection_info_.stop_reason_ == kDownloadStopReasonUnknown) {
            task->SetStopReason(kDownloadStopReasonCancelled);
        }

        // return non zero value to abort this call.
        return 1;
    }

    // to check if read timeout happend
    int64_t now = kpbase::SystemUtil::GetCPUTime();
//    LOG_DEBUG("[LibcurlDownloadTask] ProgressCallback, dltotal:%ld, dlnow:%ld, ultotal:%ld, ulnow:%ld "
//              ">>> task->last_progress_callback_ts_ms_ :%lld, task->last_dlnow_:%lld, now:%lld \n",
//              (long)dltotal, (long)dlnow, (long)ultotal, (long)ulnow,
//              task->last_progress_callback_ts_ms_, task->last_dlnow_, now);
#if defined(__ANDROID__)
    task->tcp_climing_.Update(dlnow, now);
#endif
    if (task->last_progress_callback_ts_ms_ == 0) {
        task->last_progress_callback_ts_ms_ = now;
        task->last_dlnow_ = dlnow;
    } else {
        if (task->last_dlnow_ >= dlnow) {
            // no progress
            int64_t time_diff_ms = now - task->last_progress_callback_ts_ms_;
            if (time_diff_ms >= task->options_.read_timeout_ms) {
                LOG_ERROR_DETAIL("[%d] [LibcurlDownloadTask] id:%d, ProgressCallback timeout, task->last_dlnow_:%lld ,"
                                 " dlnow:%lld , time_diff_ms:%lldms, (%llu ~ %llu)\n",
                                 task->context_id_,
                                 task->id(), task->last_dlnow_, (int64_t)dlnow, time_diff_ms,
                                 task->last_progress_callback_ts_ms_, now);
                task->SetErrorCodeAndStopReason(kResultExceptionNetDataSourceReadTimeout, kDownloadStopReasonTimeout);
                // return non zero value to abort this call.
                return 1;
            }
        } else {
            // has progress, refresh timestamp
            task->last_progress_callback_ts_ms_ = now;
            task->last_dlnow_ = dlnow;
        }
    }

    return 0;
}

int SockOptCallback(void* clientp,
                    curl_socket_t curlfd,
                    curlsocktype purpose) {
    /* This return code was added in libcurl 7.21.5 */

    LibcurlDownloadTask* task = static_cast<LibcurlDownloadTask*>(clientp);
    int ret = 0;
    int recv_len = 0;
    socklen_t opt_len = sizeof(recv_len);

    ret = getsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &recv_len, &opt_len);
    if (ret < 0) {
        LOG_ERROR("[SockOptCallback], getsockopt, original FAIL, ret:%d", ret);
    }
    task->ac_rt_info_->download_task.sock_orig_size_kb = recv_len > 0 ? recv_len / 1024 : recv_len;

    if (purpose != CURLSOCKTYPE_IPCXN) {
        // 我们只设置CURLSOCKTYPE_IPCXN相关的连接
        LOG_ERROR("[SockOptCallback], warning , purpose(%d) != CURLSOCKTYPE_IPCXN", purpose);
        task->ac_rt_info_->download_task.sock_act_size_kb = -2; //暂时不知道线上有没有这种case，先用-2在灰度上区分一下数据
        return CURL_SOCKOPT_OK;
    }

    LOG_INFO("[%d] [SockOptCallback]try to set socket buffer size:%dkb", task->context_id_, task->options_.socket_buf_size_kb);
    if (task->options_.socket_buf_size_kb > 0) {
        int set_recv_len = task->options_.socket_buf_size_kb * 1024;
        ret = setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &set_recv_len, sizeof(set_recv_len));
        if (ret < 0) {
            LOG_ERROR("[SockOptCallback], setsockopt FAIL, ret:%d", ret);
        }

        recv_len = -1;
        ret = getsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &recv_len, &opt_len);
        if (ret < 0) {
            LOG_ERROR("[SockOptCallback], getsockopt after getsockopt FAIL, ret:%d", ret);
            task->ac_rt_info_->download_task.sock_act_size_kb =
                task->ac_rt_info_->download_task.sock_orig_size_kb;
        } else {
            task->ac_rt_info_->download_task.sock_act_size_kb =
                recv_len > 0 ? recv_len / 1024 : recv_len;
        }
    } else {
        task->ac_rt_info_->download_task.sock_act_size_kb =
            task->ac_rt_info_->download_task.sock_orig_size_kb;
    }
    task->ac_rt_info_->download_task.sock_cfg_size_kb = task->options_.socket_buf_size_kb;

    return CURL_SOCKOPT_OK;
}


void LibcurlDownloadTask::CollectCurlInfoOnce() {
    if (curl_ && !curl_info_collected_) {
        long response_code;
        long redirect_count;
        char* effective_url = nullptr;
        double dns_time;
        double connect_time;
        double start_transfer_time;
        curl_off_t content_lenth  = 0;
        char* ip_address;
        long http_version;

        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_getinfo(curl_, CURLINFO_REDIRECT_COUNT, &redirect_count);
        curl_easy_getinfo(curl_, CURLINFO_EFFECTIVE_URL, &effective_url);
        curl_easy_getinfo(curl_, CURLINFO_NAMELOOKUP_TIME, &dns_time);
        curl_easy_getinfo(curl_, CURLINFO_CONNECT_TIME, &connect_time);
        curl_easy_getinfo(curl_, CURLINFO_STARTTRANSFER_TIME, &start_transfer_time);
        curl_easy_getinfo(curl_, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_lenth);
        curl_easy_getinfo(curl_, CURLINFO_PRIMARY_IP, &ip_address);
        curl_easy_getinfo(curl_, CURLINFO_HTTP_VERSION, &http_version);


        connection_info_.connection_used_time_ms = static_cast<int>(connect_time * 1000);
        connection_info_.http_dns_analyze_ms = static_cast<int>(dns_time * 1000);
        connection_info_.http_first_data_ms = static_cast<int>(start_transfer_time * 1000);
        connection_info_.ip = ip_address ? ip_address : "";
        connection_info_.response_code = (int) response_code;
        connection_info_.redirect_count = (int)redirect_count;
        connection_info_.effective_url = effective_url ? effective_url : "";
        connection_info_.content_length_from_curl_ = content_lenth;
        if (ac_rt_info_ != nullptr) {
            ac_rt_info_->download_task.http_connect_ms = connection_info_.connection_used_time_ms;
            ac_rt_info_->download_task.http_dns_analyze_ms = connection_info_.http_dns_analyze_ms;
            ac_rt_info_->download_task.http_first_data_ms = connection_info_.http_first_data_ms;
            snprintf(ac_rt_info_->download_task.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
                     connection_info_.ip.c_str());
            strncpy(ac_rt_info_->download_task.http_version,
                    http_version == CURL_HTTP_VERSION_1_1 ? "HTTP 1.1" :
                    http_version == CURL_HTTP_VERSION_1_0 ? "HTTP 1.0" : "HTTP UNKNOWN", HTTP_VERSION_MAX_LEN);
            // 针对hls多次连接，media playerlist index 1、2分别表示m3u8和ts
            if (ac_rt_info_->datasource_index <= CONNECT_INFO_COUNT) {
                CopyConnectionInfoToRuntimeInfo(ac_rt_info_->connect_infos[ac_rt_info_->datasource_index - 1], connection_info_);
            }
        }
        curl_info_collected_ = true;

        if (kVerbose) {
            LOG_DEBUG("[%d] [LibcurlDownloadTask::CollectCurlInfoOnce], connect_ms:%dms, dns_analyze:%dms, http_first_data:%dms, resp_code:%d",
                      context_id_,
                      connection_info_.connection_used_time_ms,
                      connection_info_.http_dns_analyze_ms,
                      connection_info_.http_first_data_ms,
                      connection_info_.response_code);
        }
    }
}

void LibcurlDownloadTask::SetErrorCodeAndStopReason(int error_code,
                                                    DownloadStopReason reason) {
    SetErrorCode(error_code);
    SetStopReason(reason);
}

void LibcurlDownloadTask::SetErrorCode(int error_code) {
    connection_info_.error_code = error_code;
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->download_task.error_code = error_code;
    }
}

void LibcurlDownloadTask::SetOsErrno(long os_errno) {
    connection_info_.os_errno = os_errno;
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->download_task.os_errno = os_errno;
    }
}

void LibcurlDownloadTask::SetStopReason(DownloadStopReason reason) {
    connection_info_.stop_reason_ = reason;
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->download_task.stop_reason = reason;
    }
}

void LibcurlDownloadTask::CopyConnectionInfoToRuntimeInfo(
    AwesomeCacheRuntimeInfo::ConnectInfo& ac_rt_info, ConnectionInfo& info) {
    ac_rt_info.position = spec_.position;
    ac_rt_info.length = spec_.length;
    ac_rt_info.http_connect_ms = info.connection_used_time_ms;
    ac_rt_info.http_dns_analyze_ms = info.http_dns_analyze_ms;
    ac_rt_info.http_first_data_ms = info.http_first_data_ms;
    ac_rt_info.first_data_ts = AcUtils::GetCurrentTime();
    snprintf(ac_rt_info.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
             info.ip.c_str());
}

bool LibcurlDownloadTask::IsHttpsScheme(std::string& url_str) {
    size_t offset_scheme = url_str.find("://");
    if (offset_scheme != -1) {
        std::string scheme = url_str.substr(0, offset_scheme);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
        if (scheme == "https") {
            return true;
        }
    }
    return false;
}

bool LibcurlDownloadTask::ParseAndReplaceIpWithDomain(std::string& url_str, std::string& ip) {
    regex ip_reg("((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])");
    std::string domain_str;
    size_t scheme_offset = url_str.find("://");
    if (scheme_offset != -1) {
        std::string url_body = url_str.substr(scheme_offset + 3, url_str.length());
        size_t path_offset = url_body.find("/");
        if (path_offset == -1) {
            // https://xxx.com
            domain_str = url_body.substr(0, url_str.length());
        } else {
            // https://xxx.com/path
            domain_str = url_body.substr(0, path_offset);
        }

        if (regex_match(domain_str, ip_reg)) {
            ip = domain_str;
            if (!ip.empty() && !connection_info_.host.empty()) {
                // 对于ip访问，将url中的ip替换为域名，配合CURLOPT_RESOLVE实现https ip直连
                url_str.replace(scheme_offset + 3, path_offset, connection_info_.host);
                return true;
            }
        }
    }
    return false;
}

} // cache
} // kuaishou

