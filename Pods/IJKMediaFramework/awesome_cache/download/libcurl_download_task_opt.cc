/*
 * libcurl_download_task_opt was added to support http byte-range better by
 * comparing to libcurl_download_task, we get below benefits:
 * 1. libcurl_download_task_opt use only one curl_handle(curl_easy_init once)
 *    and one thread for byte-ranges' downloading of a certain url
 * 2. libcurl_download_task needs a curl_handle and create a new thread for
 *    each byte-range's download
 */
#include "libcurl_download_task_opt.h"
#include "utility.h"
#include "ac_log.h"
#include "cache_session_listener.h"
#include <json/json.h>
#include <cache/cache_util.h>
#include "ac_utils.h"
#include "dcc_algorithm_c.h"
#include "connection_info.h"
#include <regex>
#include "libcurl_connection_reuse_manager.h"

using json = nlohmann::json;

namespace kuaishou {
namespace cache {
namespace {
static const int kClosed = 1;
static const int kConnected = 2;
static const int MAX_HTTP_X_KS_COUNT = 10;
}

size_t HeaderCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTaskOpt* task);
size_t WriteCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTaskOpt* task);
int ProgressCallback(LibcurlDownloadTaskOpt* task,
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t ultotal, curl_off_t ulnow);

LibcurlDownloadTaskOpt::LibcurlDownloadTaskOpt(const DownloadOpts& opts,
                                               AwesomeCacheRuntimeInfo* ac_rt_info) :
    options_(opts),
    curl_(nullptr),
    task_start_download_ts_ms_(0),
    task_make_connection_ts_ms_(0),
    feed_data_consume_ms_(0),
    task_total_consume_ms_(0),
    transfer_over_recorded_(false),
    state_(kClosed),
    paused_(false),
    pending_paused_(false),
    pending_resumed_(false),
    abort_(false),
    last_dlnow_(0),
    first_open_(true),
    terminate_thread_(false),
    last_progress_callback_ts_ms_(0),
    last_log_time_(0),
    last_log_size_(0),
    close_waiting(false),
    ac_rt_info_(ac_rt_info) {
    SetPriority(opts.priority);
    interrupt_callback_ = opts.interrupt_cb;
    buffer_size_ = std::min(std::max(kMinCurlBufferSizeKb, opts.curl_buffer_size_kb),
                            kMaxCurlBufferSizeKb) * KB;
    context_id_ = opts.context_id;

    if (ac_rt_info_ != nullptr) {
        AwesomeCacheRuntimeInfo_download_task_init(ac_rt_info_);
        ac_rt_info_->download_task.curl_buffer_size_kb = buffer_size_ / 1024;
        ac_rt_info_->download_task.con_timeout_ms = options_.connect_timeout_ms;
        ac_rt_info_->download_task.read_timeout_ms = options_.read_timeout_ms;
        ac_rt_info_->download_task.curl_byte_range_error = 0;
    }
}

LibcurlDownloadTaskOpt::~LibcurlDownloadTaskOpt() {
    Close();

    terminate_thread_ = true;
    open_event_.Signal();

    if (thread_.joinable()) {
        LOG_DEBUG("[%d][LibcurlDownloadTaskOpt::Close] id: %d, thread_.join()", context_id_, id());
        thread_.join();
        LOG_DEBUG("[%d][LibcurlDownloadTaskOpt::Close] id: %d, thread_.join() over", context_id_, id());
    }

    LibcurlConnectionReuseManager::Teardown(curl_);
    curl_easy_cleanup(curl_);
    curl_ = nullptr;

    LOG_DEBUG("[%d][LibcurlDownloadTaskOpt::~LibcurlDownloadTaskOpt()] id: %d \n", context_id_, id());
}

ConnectionInfo LibcurlDownloadTaskOpt::MakeConnection(const DataSpec& spec) {
    LOG_DEBUG("[%d][LibcurlDownloadTaskOpt] id: %d, spec.position: %lld, spec.length: %lld, buffer_size:%dKB",
              context_id_, id(), spec.position, spec.length, buffer_size_);
    abort_ = false;
    connection_info_.download_uuid = CacheUtil::GenerateUUID();
    connection_info_.session_uuid = options_.session_uuid.empty() ? "NULL" : options_.session_uuid;
    user_agent_ = options_.user_agent
                  + "/" + connection_info_.session_uuid
                  + "/" + connection_info_.download_uuid
                  + "/cache";

    LOG_DEBUG("[%d][session_uuid]user_agent :%s, dl_id.len:%d", context_id_, user_agent_.c_str());

    uint64_t now = kpbase::SystemUtil::GetCPUTime();
    if (!speed_cal_ ||
        (task_make_connection_ts_ms_ != 0 && now - task_make_connection_ts_ms_ > SpeedCalculator::REOPEN_COST_MS_THRESHOLD_TO_STOP)) {
        // 如果是第一次，或者请求时间不连续，则启用一次新的测速

        LOG_INFO("[%d][LibcurlDownloadTaskOpt] %ld ms pause between range requests, use new speed calculator",
                 context_id_, now - task_make_connection_ts_ms_);
        speed_cal_.reset(new SpeedCalculator());
        speed_cal_total_downloaded_size_ = 0;
    }

    spec_ = spec;
    if (input_stream_) {
        input_stream_->Close();
    }
    input_stream_ = make_shared<BlockingInputStream>(buffer_size_);
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->datasource_index++;
    }
    if (first_open_) {
        thread_ = std::thread(&LibcurlDownloadTaskOpt::Run, this);
    } else {
        // 重新初始化参数
        state_ = kClosed;
        close_waiting = false;
        connection_info_.downloaded_bytes_ = 0;
        connection_info_.connection_closed = false;
        connection_info_.download_complete_ = false;
        connection_info_.content_length_from_curl_ = kLengthUnset;
        connection_info_.error_code = 0;
        connection_info_.stop_reason_ = kDownloadStopReasonUnknown;
        connection_info_.transfer_consume_ms_ = 0;
        connection_info_.http_dns_analyze_ms = 0;
        connection_info_.http_first_data_ms = 0;
        connection_info_.connection_used_time_ms = 0;
        connection_info_.content_length = kLengthUnset;
        connection_info_.response_code = 0;
        last_progress_callback_ts_ms_ = 0;  // avoid timeout in progressCallback
        last_dlnow_ = 0;
        task_start_download_ts_ms_ = 0;
        task_make_connection_ts_ms_ = now;
        task_total_consume_ms_ = 0;
        transfer_over_recorded_ = false;
        feed_data_consume_ms_ = 0;

        open_event_.Signal();
    }


    // wait for connection open signal.
    connection_opened_.Wait();
    return connection_info_;
}

void LibcurlDownloadTaskOpt::Pause() {
    LOG_DEBUG("[%d][LibcurlDownloadTaskOpt] id: %d, pause", context_id_, id());
    //if (state_ == kConnected && !paused_ && !pending_paused_) {
    //    // handled in write callback.
    //    pending_paused_ = true;
    //}
}

void LibcurlDownloadTaskOpt::Resume() {
    LOG_DEBUG("[%d][LibcurlDownloadTaskOpt] id: %d, resume", context_id_, id());
    //if (state_ == kConnected && paused_ && !pending_resumed_) {
    //    // handled in progress callback.
    //    pending_resumed_ = true;
    //}
}

void LibcurlDownloadTaskOpt::Close() {
    input_stream_->Close();

    if (!abort_) {
        abort_ = true;
    }

    {
        std::lock_guard<std::mutex> lg(state_mutex_);
        if (state_ == kConnected) {
            LOG_DEBUG("[%d][LibcurlDownloadTaskOpt::Close] id: %d, set close_waiting to true", context_id_, id());
            close_waiting = true;
        }
    }

    if (close_waiting) {
        LOG_DEBUG("[%d][LibcurlDownloadTaskOpt::Close] id: %d, close_waiting, close_event_.Wait()", context_id_, id());
        close_event_.Wait();
        close_waiting = false;
    }
}

void LibcurlDownloadTaskOpt::ParseHeader() {
    std::string header = http_header_;
    LOG_DEBUG("[%d][LibcurlDownloadTaskOptCallBack::ParseHeader] id: %d, http_header:%s",
              context_id_, id(), http_header_.c_str());
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
        LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack::ParseHeader] id: %d, fail, have not found any return_str \n", context_id_, id());
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
    }
}

void LibcurlDownloadTaskOpt::GetHttpXKsJsonString() {
    int size = (int)http_x_ks_headers_.size();

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

std::shared_ptr<InputStream> LibcurlDownloadTaskOpt::GetInputStream() {
    return input_stream_;
}

const ConnectionInfo& LibcurlDownloadTaskOpt::GetConnectionInfo() {
    return connection_info_;
}

bool LibcurlDownloadTaskOpt::IsInterrupted() {
    return AwesomeCacheInterruptCB_is_interrupted(&interrupt_callback_);
}

void LibcurlDownloadTaskOpt::UpdateDownloadedSizeFromCurl() {
    if (transfer_over_recorded_) {
        return;
    }
    curl_off_t download_size = 0;
    curl_easy_getinfo(curl_, CURLINFO_SIZE_DOWNLOAD_T, &download_size);
    connection_info_.UpdateDownloadedSize(download_size);
    UpdateDownloadBytes();
    UpdateSpeedCalculator();
    int64_t current_time = AcUtils::GetCurrentTime();
    if (current_time - last_log_time_ > KDownload_Log_Interval_Ms &&
        download_size - last_log_size_ > KDownload_Log_Bytes_Threshold) {
        last_log_size_ = download_size;
        last_log_time_ = current_time;
        LOG_INFO("[%d][DownloadRecord] id: %d, time:%lld, task_downloadsize:%lld, "
                 "total_downloadsize:%lld",
                 context_id_, id(), current_time, download_size,
                 (ac_rt_info_ != nullptr) ? ac_rt_info_->http_ds.download_bytes : 0);
    }
}

void LibcurlDownloadTaskOpt::UpdateDownloadBytes() {
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->http_ds.download_bytes = ac_rt_info_->http_ds.task_downloaded_bytes + connection_info_.GetDownloadedBytes();
    }
}

void LibcurlDownloadTaskOpt:: onTransferOver() {
    if (transfer_over_recorded_) {
        return;
    }
    uint64_t end = kpbase::SystemUtil::GetCPUTime();
    task_total_consume_ms_ = (int)(end - task_start_download_ts_ms_);
    transfer_over_recorded_ = true;

    // 这个数据很重要，最终要通过onDownloadStop返给cdn
    connection_info_.transfer_consume_ms_ = task_total_consume_ms_ - feed_data_consume_ms_;
    if (connection_info_.transfer_consume_ms_ < 0) {
        connection_info_.transfer_consume_ms_ = 0;
    }

    if (ac_rt_info_ != nullptr) {
        AwesomeCacheRuntimeInfo_download_task_end(ac_rt_info_);
        ac_rt_info_->download_task.download_total_cost_ms = task_total_consume_ms_;
        ac_rt_info_->http_ds.task_downloaded_bytes = ac_rt_info_->http_ds.task_downloaded_bytes + connection_info_.downloaded_bytes_;
    }
    if (speed_cal_->IsMarkValid()) {
        DccAlgorithm_update_speed_mark(speed_cal_->GetMarkSpeedKbps());
    }
    speed_cal_total_downloaded_size_ += connection_info_.downloaded_bytes_;
}

void LibcurlDownloadTaskOpt::UpdateSpeedCalculator() {
    speed_cal_->Update(speed_cal_total_downloaded_size_ + connection_info_.downloaded_bytes_); // total downloaded size of multiple range requests, in the lifetime of speed_cal
    if (ac_rt_info_ != nullptr) {
        ac_rt_info_->download_task.speed_cal_current_speed_index++;
        ac_rt_info_->download_task.speed_cal_current_speed_kbps = speed_cal_->GetCurrentSpeedKbps();
        ac_rt_info_->download_task.speed_cal_avg_speed_kbps  = speed_cal_->GetAvgSpeedKbps();
        ac_rt_info_->download_task.speed_cal_mark_speed_kbps = speed_cal_->GetMarkSpeedKbps();
    }
}

void LibcurlDownloadTaskOpt::CopyConnectionInfoToRuntimeInfo(AwesomeCacheRuntimeInfo::ConnectInfo& ac_rt_info, ConnectionInfo& info) {
    ac_rt_info.position = spec_.position;
    ac_rt_info.length = spec_.length;
    ac_rt_info.http_connect_ms = info.connection_used_time_ms;
    ac_rt_info.http_dns_analyze_ms = info.http_dns_analyze_ms;
    ac_rt_info.http_first_data_ms = info.http_first_data_ms;
    ac_rt_info.first_data_ts = AcUtils::GetCurrentTime();
    snprintf(ac_rt_info.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
             info.ip.c_str());
}

void LibcurlDownloadTaskOpt::Run() {
    // To support byte-range request with DataSource::Open, https://curl.haxx.se/libcurl/c/curl_easy_perform.html
    if (!curl_) {
        curl_ = curl_easy_init();
        LibcurlConnectionReuseManager::Setup(curl_, this->options_);
    }
    long response_code = 0;
    double connect_time_ = 0;
    double dns_time = 0;
    double start_transfer_time_ = 0;
    long os_errno = 0;

    AcUtils::SetThreadName("LibcurlDownloadTaskOpt");
    LOG_DEBUG("[%d][LibcurlDownloadTaskOpt] id: %d, curl_easy_init()", context_id_, id());
    while (!terminate_thread_) {
        if (!first_open_) {
            LOG_DEBUG("[%d][LibcurlDownloadTaskOpt] id: %d, open_event_ wait", context_id_, id());
            open_event_.Wait();
        }
        if (terminate_thread_) {
            break;
        }

        if (curl_) {
            connection_info_.uri = spec_.uri;
            curl_easy_setopt(curl_, CURLOPT_URL, spec_.uri.c_str());
            curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl_, CURLOPT_USERAGENT, user_agent_.c_str());
            curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);

            curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
            // Option for HTTP Connect Timeout   // 注意，这个接口的的单位是秒
            curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, options_.connect_timeout_ms / 1000);
            // http://www.cnblogs.com/edgeyang/articles/3722035.html
            curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);

            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id:%d, connect_timeout_ms :%d ms, read_timeout_ms:%dms",
                     context_id_, id(), options_.connect_timeout_ms, options_.read_timeout_ms);

            // add http headers.
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

            if (spec_.position != 0 || (spec_.length != kLengthUnset && spec_.length > 0)) {
                connection_info_.range_request_start = spec_.position;
                std::string range_request = "Range: bytes=" + kpbase::StringUtil::Int2Str(spec_.position) + "-";
                if (spec_.length != kLengthUnset && spec_.length > 0) {
                    int64_t range_end = spec_.position + spec_.length - 1;
                    range_request += kpbase::StringUtil::Int2Str(range_end);
                    connection_info_.range_request_end = range_end;
                }
                LOG_DEBUG("[%d][LibcurlDownloadTaskOpt] range_request: %s", context_id_, range_request.c_str());
                header_list = curl_slist_append(header_list, range_request.c_str());
            }
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
            std::regex reg("Accept-Encoding:[\t ]*(.*)\r\n");
            smatch m;
            regex_search(options_.headers, m, reg);
            if (m[1].matched) {
                curl_easy_setopt(curl_, CURLOPT_ACCEPT_ENCODING, m[1].str().c_str());
            }

            // progress callback.
            curl_easy_setopt(curl_, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt(curl_, CURLOPT_XFERINFODATA, this);
            // header write callback
            curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
            curl_easy_setopt(curl_, CURLOPT_HEADERDATA, this);
            // write callback.
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);

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
            onTransferOver();

            curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_getinfo(curl_, CURLINFO_NAMELOOKUP_TIME, &dns_time);
            curl_easy_getinfo(curl_, CURLINFO_CONNECT_TIME, &connect_time_);
            curl_easy_getinfo(curl_, CURLINFO_STARTTRANSFER_TIME, &start_transfer_time_);
            curl_easy_getinfo(curl_, CURLINFO_OS_ERRNO, &os_errno);
            connection_info_.os_errno = os_errno;
            if (ac_rt_info_ != nullptr) {
                ac_rt_info_->download_task.os_errno = os_errno;
            }
            connection_info_.response_code = (int)response_code;

            if (curl_ret != CURLE_OK) {
                // 这快StopReason的逻辑谨慎修改，因为涉及到app层来决定是否需要切换cdn重试，该逻辑会影响卡顿数据
                if (curl_ret == CURLE_OPERATION_TIMEDOUT) {
                    // timeout 单独作为一个reason
                    connection_info_.stop_reason_ = kDownloadStopReasonTimeout;
                } else {
                    if (connection_info_.stop_reason_ != kDownloadStopReasonCancelled
                        && connection_info_.stop_reason_ != kDownloadStopReasonFinished) {
                        connection_info_.stop_reason_ = kDownloadStopReasonFailed;
                    }
                }

                // 如果之前 connection_info_.error_code已经被赋值过了，就不再次赋值了
                if (connection_info_.error_code == 0) {
                    int error_code = (curl_ret > 0 ? -curl_ret : curl_ret) + kLibcurlErrorBase;
                    connection_info_.error_code = error_code;
                    if (connection_info_.stop_reason_ != kDownloadStopReasonCancelled
                        && connection_info_.stop_reason_ != kDownloadStopReasonFinished) {
                        connection_info_.stop_reason_ = kDownloadStopReasonFailed;
                    }
                }
            } else if (!connection_info_.IsResponseCodeSuccess()) {
                LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOpt::Run] id: %d, after curl_easy_perform, responseCode:%d is failure code \n",
                                 context_id_, id(), connection_info_.response_code);
                connection_info_.error_code = kHttpInvalidResponseCodeBase - (int)response_code;

                if (connection_info_.stop_reason_ == kDownloadStopReasonUnknown) {
                    connection_info_.stop_reason_ = kDownloadStopReasonFailed;
                }
            } else {
                if (connection_info_.stop_reason_ == kDownloadStopReasonUnknown) {
                    connection_info_.stop_reason_ = kDownloadStopReasonFinished;
                }
            }
            if (ac_rt_info_ != nullptr) {
                ac_rt_info_->download_task.stop_reason = connection_info_.stop_reason_;
            }
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id: %d, after curl_easy_perform : \n", context_id_, id());
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id: %d, connection_info_.error_code：%d, stop_reason:%s, response_code:%ld \n",
                     context_id_, id(), connection_info_.error_code, CacheSessionListener::DownloadStopReasonToString(connection_info_.stop_reason_), response_code);
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id: %d, total_consume_ms_:%dms, transfer_consume_ms_:%d, dl_speed:%d \n",
                     context_id_, id(), task_total_consume_ms_, connection_info_.transfer_consume_ms_, connection_info_.GetAvgDownloadSpeed());
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id: %d, spec_.position:%lld, downloaded/content_length: (%lld/%lld) \n ",
                     context_id_, id(), spec_.position, connection_info_.downloaded_bytes_, connection_info_.content_length,
                     connection_info_.GetUnDownloaedBytes());
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id: %d, ip:%s,  uri:%s \n",
                     context_id_, id(), connection_info_.ip.c_str(), spec_.uri.c_str());
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id: %d,  connection_used_time_ms: %d\n",
                     context_id_, id(), connection_info_.connection_used_time_ms);

            curl_slist_free_all(header_list);
        } else {
            LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOpt::Run] id:%d, curl_ = NULL, curl_easy_init fail \n", context_id_, id());
            connection_info_.error_code = kResultExceptionHttpDataSourceCurlInitFail;
            connection_info_.stop_reason_ = kDownloadStopReasonFailed;
        }

        // 要放在connection_opened_.Signal()的前面，避免在MakeConnection返回的时候，first_open_还没被设置成false.
        first_open_ = false;

        // Notify input stream end of stream.
        input_stream_->EndOfStream(connection_info_.error_code);

        if (state_ == kClosed) {
            LOG_DEBUG("[%d][LibcurlDownloadTaskOpt::Run] id:%d connection never opened", context_id_, id());
            connection_info_.content_length = kLengthUnset;
            connection_info_.http_dns_analyze_ms = (int)(dns_time * 1000);
            connection_info_.connection_used_time_ms = (int)(connect_time_ * 1000);
            connection_info_.http_first_data_ms = (int)(start_transfer_time_ * 1000);
            connection_info_.response_code = (int)response_code;

            connection_opened_.Signal();
        }

        {
            std::lock_guard<std::mutex> lg(state_mutex_);
            state_ = kClosed;
        }

        connection_info_.connection_closed = true;
        if (close_waiting) {
            LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id:%d close_event_ signal", context_id_, id());
            close_event_.Signal();
        }
        LOG_INFO("[%d][LibcurlDownloadTaskOpt::Run] id:%d COMPLETE", context_id_, id());
    }
}


size_t HeaderCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTaskOpt* task) {
//    LOG_DEBUG("[LibcurlDownloadTaskOptCallBack] HeaderCallback id:%d, size:%lu, nitems: %lu, buffer: %s \n",
//              task->id(), size, nitems, buffer);
    task->http_header_.append(buffer, size * nitems);
    return size * nitems;
}

size_t WriteCallback(char* buffer, size_t size, size_t nitems, LibcurlDownloadTaskOpt* task) {
//    LOG_DEBUG("[LibcurlDownloadTaskOptCallBack] WriteCallback id:%d, size: %ld, task->downloaded_bytes_: %llu \n",
//              task->id(), (long)(size * nitems), task->connection_info_.downloaded_bytes_);
    // Handle make connection open.
    if (task->state_ == kClosed) {
        {
            std::lock_guard<std::mutex> lg(task->state_mutex_);
            task->state_ = kConnected;
        }

        // Parse Header.
        task->ParseHeader();
        // fill connection info.
        long response_code;
        long redirect_count;
        char* effective_url = nullptr;
        double dns_time;
        double connect_time;
        double start_transfer_time;
        char* ip_address = nullptr;
        long http_version;

        curl_easy_getinfo(task->curl_, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_getinfo(task->curl_, CURLINFO_REDIRECT_COUNT, &redirect_count);
        curl_easy_getinfo(task->curl_, CURLINFO_EFFECTIVE_URL, &effective_url);
        curl_easy_getinfo(task->curl_, CURLINFO_NAMELOOKUP_TIME, &dns_time);
        curl_easy_getinfo(task->curl_, CURLINFO_CONNECT_TIME, &connect_time);
        curl_easy_getinfo(task->curl_, CURLINFO_PRIMARY_IP, &ip_address);
        curl_easy_getinfo(task->curl_, CURLINFO_STARTTRANSFER_TIME, &start_transfer_time);
        curl_easy_getinfo(task->curl_, CURLINFO_HTTP_VERSION, &http_version);
        task->connection_info_.connection_used_time_ms = connect_time * 1000;
        task->connection_info_.http_dns_analyze_ms = dns_time * 1000;
        task->connection_info_.http_first_data_ms = start_transfer_time * 1000;
        task->connection_info_.ip = ip_address ? ip_address : "";

        snprintf(task->ac_rt_info_->download_task.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
                 task->connection_info_.ip.c_str());
        task->ac_rt_info_->download_task.http_connect_ms = task->connection_info_.connection_used_time_ms;
        task->ac_rt_info_->download_task.http_dns_analyze_ms = task->connection_info_.http_dns_analyze_ms;
        task->ac_rt_info_->download_task.http_first_data_ms = task->connection_info_.http_first_data_ms;
        task->ac_rt_info_->download_task.feed_data_consume_ms_ = task->connection_info_.http_first_data_ms;
        strncpy(task->ac_rt_info_->download_task.http_version,
                http_version == CURL_HTTP_VERSION_1_1 ? "HTTP 1.1" :
                http_version == CURL_HTTP_VERSION_1_0 ? "HTTP 1.0" : "HTTP UNKNOWN", HTTP_VERSION_MAX_LEN);
        //多次连接，分别记录链接耗时
        if (task->ac_rt_info_->datasource_index <= CONNECT_INFO_COUNT) {
            task->CopyConnectionInfoToRuntimeInfo(task->ac_rt_info_->connect_infos[task->ac_rt_info_->datasource_index - 1], task->connection_info_);
        }

        task->connection_info_.response_code = (int)response_code;
        task->connection_info_.redirect_count = (int)redirect_count;
        task->connection_info_.effective_url = effective_url ? effective_url : "";
        // responce_code 判断错误
        if (!task->connection_info_.IsResponseCodeSuccess()) {
            task->connection_info_.error_code = kHttpInvalidResponseCodeBase - (int)response_code;
            task->connection_info_.stop_reason_ = kDownloadStopReasonFailed;
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
            if (task->http_headers_.find("content-range") == task->http_headers_.end()) {
                task->connection_info_.need_drop_bytes_ = task->spec_.position;
                LOG_DEBUG("[%d][LibcurlDownloadTaskOptCallBack] need drop data bytes: %d",
                          task->context_id_, task->connection_info_.need_drop_bytes_);
            }
        }

        int64_t content_length = kLengthUnset;
        if (task->http_headers_.find("content-length") != task->http_headers_.end()) {
            auto maybe_length = kpbase::StringUtil::Str2Int(task->http_headers_["content-length"]);
            if (!maybe_length.IsNull()) {
                content_length = maybe_length.Value();
                LOG_DEBUG("[%d][LibcurlDownloadTaskOptCallBack] content_length: %lld",
                          task->context_id_, content_length);
                if (content_length <= task->connection_info_.need_drop_bytes_) {
                    LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack] content-length Invalid , error_code = "
                                     "kDownloadStopReasonContentLengthInvalid, curl content_length:%lld, drop data length: %d \n",
                                     task->context_id_, content_length, task->connection_info_.need_drop_bytes_);
                    if (content_length <= 0) {
                        task->connection_info_.error_code = kResultExceptionHttpDataSourceInvalidContentLength;
                    } else {
                        task->connection_info_.error_code = kResultExceptionHttpDataSourceInvalidContentLengthForDrop;
                    }
                    task->connection_info_.stop_reason_ = kDownloadStopReasonContentLengthInvalid;
                }
            } else {
                LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack] content-length not found , error_code = kDownloadStopReasonNoContentLength",
                                 task->context_id_);
                task->connection_info_.error_code = kResultExceptionHttpDataSourceNoContentLength;
                task->connection_info_.stop_reason_ = kDownloadStopReasonNoContentLength;
            }
        } else {
            LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack] content-length not found , error_code = kDownloadStopReasonNoContentLength",
                             task->context_id_);
            task->connection_info_.error_code = kResultExceptionHttpDataSourceNoContentLength;
            task->connection_info_.stop_reason_  = kDownloadStopReasonNoContentLength;
        }

        if (task->http_headers_.find("content-range") != task->http_headers_.end()) {
            std::string content_range = task->http_headers_["content-range"];
            std::regex reg1("bytes (\\d+)-(\\d+)/(\\d+)");
            smatch m;
            regex_match(content_range, m, reg1);
            int64_t byte_range_start_pos = m[1].matched ? kpbase::StringUtil::Str2Int(m[1].str()).Value() : -1;
            int64_t byte_range_end_pos = m[2].matched ? kpbase::StringUtil::Str2Int(m[2].str()).Value() : -1;
            task->connection_info_.range_response_start = byte_range_start_pos;
            task->connection_info_.range_response_end = byte_range_end_pos;
            task->connection_info_.file_length = m[3].matched ? kpbase::StringUtil::Str2Int(m[3].str()).Value() : -1;

            LOG_DEBUG("[%d][LibcurlDownloadTaskOptCallBack] content_range: %s, file_len: %lld",
                      task->context_id_, content_range.c_str(), task->connection_info_.file_length);

            //检查response的range是否与请求的一致
            if (byte_range_start_pos != -1) {
                LOG_DEBUG("[%d][LibcurlDownloadTaskOptCallBack] content_range: %s, byte_range_start_pos: %d",
                          task->context_id_, content_range.c_str(), byte_range_start_pos);
                int64_t range_len = byte_range_end_pos - byte_range_start_pos + 1;
                if (task->spec_.position != byte_range_start_pos ||
                    (content_length != kLengthUnset && range_len != content_length)) {
                    task->ac_rt_info_->download_task.curl_byte_range_error++;
                    LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack] byte_range invalid task->spec_.position: %lld",
                                     task->context_id_, task->spec_.position);
                    LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack] range_len: %lld, content_length: %lld",
                                     task->context_id_, range_len, content_length);
                    task->connection_info_.stop_reason_ = kDownloadStopReasonByteRangeInvalid;
                    task->connection_info_.error_code = kResultExceptionHttpDataSourceByteRangeInvalid;
                }
            }
        }

        task->connection_info_.content_length = content_length - task->connection_info_.need_drop_bytes_;
        task->connection_info_.content_length_from_curl_ = task->connection_info_.content_length;
        // notify connected.
        task->first_open_ = false;
        task->connection_opened_.Signal();

        task->ac_rt_info_->download_task.error_code = task->connection_info_.error_code;
        task->ac_rt_info_->download_task.stop_reason = task->connection_info_.stop_reason_;

        if (task->connection_info_.error_code != 0) {
            LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOptCallBack] id: %d, error_code = %d, set abort_ = true;",
                             task->context_id_, task->id(), task->connection_info_.error_code);
            task->UpdateDownloadedSizeFromCurl();
            task->onTransferOver();
            task->abort_ = true;
            return CURL_WRITEFUNC_PAUSE;
        }
    }
    // handle pause logic.
    //if (!task->paused_ && task->pending_paused_) {
    //    task->pending_paused_ = false;
    //    task->paused_ = true;
    //    return CURL_WRITEFUNC_PAUSE;
    //}

    int32_t len = (int32_t)(size * nitems - task->connection_info_.need_drop_bytes_);
    if (len > 0) {
        // 这里必须提前自增，因为外部拿到数据的时候，会直接参考这个数据
        task->connection_info_.downloaded_bytes_  += len;
        task->UpdateDownloadedSizeFromCurl();
        bool download_complete = task->connection_info_.IsDownloadComplete();
        if (download_complete) {
            task->connection_info_.stop_reason_ = kDownloadStopReasonFinished;
            task->onTransferOver();
        }

        int64_t feed_start = kpbase::SystemUtil::GetCPUTime();
        int32_t input_stream_used_bytes = 0;
        task->input_stream_->FeedDataSync((uint8_t*) buffer + task->connection_info_.need_drop_bytes_,
                                          len, input_stream_used_bytes);
        if (download_complete) {
            task->input_stream_->EndOfStream(0);
        }
        int64_t feed_end = kpbase::SystemUtil::GetCPUTime();
        int feed_cost_ms = (int)(feed_end - feed_start);
        task->feed_data_consume_ms_ += feed_cost_ms;

        if (feed_cost_ms > 50 && !task->speed_cal_->IsStoped()) {
            // Mark (stop) current kbps if input_stream is blocked
            LOG_INFO("[%d] [dccAlg][SpeedCalculator::Update], feed_cost_ms = %d", task->context_id_, feed_cost_ms);
            task->speed_cal_->Stop();
        }

        task->connection_info_.droped_bytes_ += task->connection_info_.need_drop_bytes_;
        task->connection_info_.need_drop_bytes_ = 0;

        task->ac_rt_info_->download_task.feed_data_consume_ms_ = task->feed_data_consume_ms_;
        task->ac_rt_info_->download_task.download_total_drop_bytes = task->connection_info_.droped_bytes_;
    } else {
        task->connection_info_.need_drop_bytes_ -= size * nitems;
        task->connection_info_.droped_bytes_ +=  size * nitems;
    }

    task->ac_rt_info_->download_task.error_code = task->connection_info_.error_code;
    task->ac_rt_info_->download_task.stop_reason = task->connection_info_.stop_reason_;

    return size * nitems;
}

int ProgressCallback(LibcurlDownloadTaskOpt* task,
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t ultotal, curl_off_t ulnow) {
//    LOG_DEBUG("[LibcurlDownloadTaskOptCallBack] ProgressCallback id:%d\n", task->id());
    bool interrupted = task->IsInterrupted();
    if (task->abort_ || interrupted) {
        LOG_INFO("[%d][LibcurlDownloadTaskOptCallBack::ProgressCallback] id:%d, task->IsInterrupted():%d\n",
                 task->context_id_, task->id(), interrupted);
        LOG_INFO("[%d][LibcurlDownloadTaskOptCallBack::ProgressCallback] error_code_:%d, stop_reason:%d \n",
                 task->context_id_, task->connection_info_.error_code, task->connection_info_.stop_reason_);

        if (task->connection_info_.error_code == 0 && task->connection_info_.stop_reason_ == kDownloadStopReasonUnknown) {
            task->ac_rt_info_->download_task.stop_reason = task->connection_info_.stop_reason_ = kDownloadStopReasonCancelled;
        }

        // return non zero value to abort this call.
        return 1;
    }

    uint64_t now = kpbase::SystemUtil::GetCPUTime();
    // handle resume.
    //if (task->paused_ && task->pending_resumed_) {
    //    curl_easy_pause(task->curl_, CURLPAUSE_CONT);
    //    task->paused_ = false;
    //    task->pending_resumed_ = false;
    //    task->last_progress_callback_ts_ms_ = now; // need to refresh timestamp
    //    //OnDownloadResumed
    //}
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
                LOG_ERROR_DETAIL("[%d][LibcurlDownloadTaskOpt] ProgressCallback time out, task->last_dlnow_:%lld , "
                                 "dlnow:%lld , time_diff_ms:%lldms, (%llu ~ %llu)\n",
                                 task->context_id_, task->last_dlnow_, (int64_t)dlnow, time_diff_ms,
                                 task->last_progress_callback_ts_ms_, now);
                task->connection_info_.error_code = kResultExceptionNetDataSourceReadTimeout;
                task->connection_info_.stop_reason_ = kDownloadStopReasonTimeout;

                task->ac_rt_info_->download_task.error_code = task->connection_info_.error_code;
                task->ac_rt_info_->download_task.stop_reason = task->connection_info_.stop_reason_;
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

} // cache
} // kuaishou

