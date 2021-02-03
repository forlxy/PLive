//
// Created by MarshallShuai on 2019-06-26.
//

#include <iostream>
#include <download/libcurl_connection_reuse_manager.h>
#include <cache/cache_util.h>
#include <utility.h>
#include <regex>
#include <include/dcc_algorithm_c.h>
#include <include/cache_session_listener.h>
#include "scope_curl_http_task.h"
#include "ac_log.h"
#include "ac_utils.h"
#include "abr/abr_engine.h"
#include "http_task_header_utils.h"
#include "http_task_progress_helper.h"

static const bool kVerbose = false;

namespace kuaishou {
namespace cache {

ScopeCurlHttpTask::ScopeCurlHttpTask(
    const kuaishou::cache::DownloadOpts& opts, ScopeTaskListener* listener, AwesomeCacheRuntimeInfo* ac_rt_info) :
    task_listener_(listener),
    options_(opts),
    interrupt_callback_(opts.interrupt_cb),
    context_id_(opts.context_id),
    last_error_(kResultOK),
    http_header_parsed_(false),
    stop_reason_(kDownloadStopReasonUnset),
    state_(ScopeCurlHttpTask::State_Inited),
    curl_(nullptr),
    curl_header_list_(nullptr),
    ac_rt_info_(ac_rt_info),
    last_dlnow_(0),
    last_progress_callback_ts_ms_(0),
    recv_valid_bytes_(0),
    server_not_support_range_(false),
    to_skip_bytes_(0),
    skipped_bytes_(0) {

    static int id_index = 0;
    id_ = id_index++;

    abort_ = false;
    interrupt_callback_ = opts.interrupt_cb;
    if (ac_rt_info_) {
        ac_rt_info_->download_task.con_timeout_ms = options_.connect_timeout_ms;
        ac_rt_info_->download_task.read_timeout_ms = options_.read_timeout_ms;
    }

    progress_helper_.reset(new HttpTaskProgressHelper(id_, opts, &connection_info_, ac_rt_info));
}

ScopeCurlHttpTask::~ScopeCurlHttpTask() {
//    LOG_VERBOSE("[ScopeCurlHttpTask::~ScopeCurlHttpTask]");
    WaitForTaskFinish();
}

int64_t ScopeCurlHttpTask::Open(const DataSpec& spec) {
    // reset error
    last_error_ = kResultOK;

    if (kVerbose) {
        LOG_DEBUG("[%d][id:%d][ScopeCurlHttpTask::Open] position:%lld ~ %lld, length:%lld",
                  context_id_, id(), spec.position,
                  spec.length > 0 ? (spec.position + spec.length - 1) : -1,
                  spec.length);
    }

    spec_ = spec;

    connection_info_.download_uuid = CacheUtil::GenerateUUID();
    connection_info_.session_uuid = options_.session_uuid.empty() ? "NULL" : options_.session_uuid;
    user_agent_ = options_.user_agent
                  + "/" + connection_info_.session_uuid
                  + "/" + connection_info_.download_uuid
                  + "/cache";

    int ret = pthread_create(&thread_id_, nullptr, &ScopeCurlHttpTask::DownloadThread, this);
    if (ret != 0) {
        LOG_ERROR("[%d][id:%ld][ScopeCurlHttpTask::Open], pthread_create fail",
                  context_id_, id());
        thread_joined = true;
        return kCurlHttpStartThreadFail;
    } else {
        thread_joined = false;
    }
    // 如果下载出错（比如header不合法），last_error_在WriteCallback里会更新
    if (last_error_ < 0) {
        return last_error_;
    } else {
        return kResultOK;
    }
}

void ScopeCurlHttpTask::Close() {
    // LOG_INFO("[%d][id:%d][ScopeCurlHttpTask::Close]", context_id_, id());
    abort_ = true;
    WaitForTaskFinish();
}

void ScopeCurlHttpTask::SetupCurl() {
    if (!curl_) {
        LOG_ERROR("[%d][id:%d][ScopeCurlHttpTask::SetupCurl], curl is null! return", context_id_, id());
        return;
    }

    LibcurlConnectionReuseManager::Setup(curl_, this->options_);

    connection_info_.uri = spec_.uri;

    curl_easy_setopt(curl_, CURLOPT_URL, spec_.uri.c_str());
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
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, options_.connect_timeout_ms);
    // http://www.cnblogs.com/edgeyang/articles/3722035.html
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);

    if (options_.http_proxy_address.length() > 0) {
        curl_easy_setopt(curl_, CURLOPT_PROXY, options_.http_proxy_address.c_str());
        curl_easy_setopt(curl_, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
        LOG_VERBOSE("[%d][id:%d][ScopeCurlHttpTask::SetupCurl], use http_proxy_address:%s",
                    context_id_, id(), options_.http_proxy_address.c_str());
    }

    http::SetRequestHeaders(spec_, options_, &connection_info_, [&](std::string const & name, std::string const & value) {
        if (name == "Range" && MockServerRangeNotSupport())
            return;
        if (kVerbose) {
            LOG_DEBUG("[SetRequestHeaders]: name:%s, value:%s", name.c_str(), value.c_str());
        }
        curl_header_list_ = curl_slist_append(curl_header_list_, (name + ": " + value).c_str());
    });

    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curl_header_list_);

    curl_easy_setopt(curl_, CURLOPT_SOCKOPTFUNCTION, &ScopeCurlHttpTask::SockOptCallback);
    curl_easy_setopt(curl_, CURLOPT_SOCKOPTDATA, this);

    // progress callback.
    curl_easy_setopt(curl_, CURLOPT_XFERINFOFUNCTION, &ScopeCurlHttpTask::ProgressCallback);
    curl_easy_setopt(curl_, CURLOPT_XFERINFODATA, this);
    // header write callback
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, &ScopeCurlHttpTask::HeaderCallback);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, this);
    // write callback.
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &ScopeCurlHttpTask::WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);

    if (options_.max_speed_kbps > 0) {
        curl_off_t max_speed = options_.max_speed_kbps * 1024 / 8;
        curl_easy_setopt(curl_, CURLOPT_MAX_RECV_SPEED_LARGE, max_speed);//bytes/s
    }

    if (ac_rt_info_) {
        AwesomeCacheRuntimeInfo_download_task_set_config_user_agent(ac_rt_info_, user_agent_.c_str());
    }
}

void ScopeCurlHttpTask::CleanupCurl() {
    // always clean up curl.
    if (curl_) {
        LibcurlConnectionReuseManager::Teardown(curl_);
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }

    // 确认下放这里会不会有问题
    if (curl_header_list_) {
        curl_slist_free_all(curl_header_list_);
        curl_header_list_ = nullptr;
    }
}

void ScopeCurlHttpTask::DoDownload() {
    connection_info_.download_uuid = CacheUtil::GenerateUUID();
    connection_info_.session_uuid = options_.session_uuid.empty() ? "NULL"
                                    : options_.session_uuid;
    user_agent_ = options_.user_agent
                  + "/" + connection_info_.session_uuid
                  + "/" + connection_info_.download_uuid
                  + "/cache";

    curl_ = curl_easy_init();

    AcUtils::SetThreadName("ScopeCurlHttpTask");

    if (curl_) {
        SetupCurl();

        progress_helper_->OnStart();
        int curl_ret = curl_easy_perform(curl_);
        progress_helper_->OnFinish();

        if (curl_ret != CURLE_OK && curl_ret != CURLE_ABORTED_BY_CALLBACK) {
            LOG_ERROR("[%d][id:%d][ScopeCurlHttpTask::DoDownload]curl_easy_perform return with error:%d, last_error is:%d",
                      context_id_, id(), curl_ret, last_error_);
        }

        CollectCurlInfo();

        CheckErrorAfterCurlPerform(curl_ret);

        if (ac_rt_info_) {
            ac_rt_info_->download_task.curl_ret = curl_ret;
            ac_rt_info_->download_task.download_total_cost_ms = connection_info_.transfer_consume_ms;
            ac_rt_info_->download_task.downloaded_bytes = connection_info_.downloaded_bytes_from_curl;
            ac_rt_info_->download_task.recv_valid_bytes = recv_valid_bytes_;
            ac_rt_info_->cache_v2_info.skip_scope_cnt += server_not_support_range_ ? 1 : 0;
            ac_rt_info_->cache_v2_info.skip_total_bytes += skipped_bytes_;
        }

        LOG_INFO("[%d] [ScopeCurlHttpTask::DoDownload] id: %d, after curl_easy_perform, ip:%s,  uri:%s \n",
                 options_.context_id, id(), connection_info_.ip.c_str(), spec_.uri.c_str());
        LOG_INFO("[%d] [ScopeCurlHttpTask::DoDownload] id: %d, error_code：%d, stop_reason:%s, response_code:%d \n",
                 options_.context_id, id(), last_error_, CacheSessionListener::DownloadStopReasonToString(stop_reason_), connection_info_.response_code);
        LOG_INFO("[%d] [ScopeCurlHttpTask::DoDownload] id: %d, transfer_consume_ms_:%d, dl_speed_kpbs:%lld \n",
                 options_.context_id, id(), connection_info_.transfer_consume_ms, connection_info_.GetAvgDownloadSpeedkbps());
        // 下面几条日志是分析cdn上报用的，勿轻易改格式
        LOG_INFO("[%d] [ScopeCurlHttpTask::DoDownload] id: %d, spec_.position:%lld, content_length:%lld, download:%lld, un_downloaed:%lld",
                 context_id_, id(),
                 spec_.position,
                 connection_info_.content_length,
                 connection_info_.downloaded_bytes_from_curl,
                 connection_info_.content_length - connection_info_.downloaded_bytes_from_curl);

        CleanupCurl();
    } else {
        LOG_ERROR_DETAIL("[%d][id:%d][ScopeCurlHttpTask::DoDownload] curl_ = NULL, curl_easy_init fail \n",
                         context_id_, id());
        SetErrorCode(kResultExceptionHttpDataSourceCurlInitFail);
        SetStopReason(kDownloadStopReasonFailed);
    }

    if (ac_rt_info_) {
        ac_rt_info_->download_task.error_code = last_error_;
        ac_rt_info_->download_task.stop_reason = stop_reason_;
    }

    if (task_listener_) {
        task_listener_->OnDownloadComplete(last_error_, stop_reason_);
    } else {
        LOG_WARN("task_listener_ is null");
    }

    // 暂留log
    // LOG_VERBOSE("[%d][id:%d][ScopeCurlHttpTask::DoDownload] thread exit", context_id_, id());
}

void ScopeCurlHttpTask::CheckErrorAfterCurlPerform(int curl_ret) {
    if (curl_ret == CURLE_OK) {
        // CURL顺利执行完，没遇到错误，也没被abort
        if (CheckResponseCode() != kResultOK) {
            // 错误码和stop reason已经在 CheckResponseCode 设置好
            return;
        } else {
            // do nothing
            stop_reason_ = kDownloadStopReasonFinished;
        }
    } else if (curl_ret == CURLE_ABORTED_BY_CALLBACK) {
        if (connection_info_.IsDownloadComplete()) {
            stop_reason_ = kDownloadStopReasonFinished;
        } else {
            // 正常cancel场景， donothing，错误码和/stop_reason已经设置好了
            if (last_error_ == kResultOK) {
                SetErrorCode(-curl_ret  + kLibcurlErrorBase);
                stop_reason_ = kDownloadStopReasonCancelled;
            } else {
                // already has last_error_, no need to update or override
                stop_reason_ = kDownloadStopReasonFailed;
            }
        }
    } else {
        SetErrorCode(-curl_ret  + kLibcurlErrorBase);
        SetStopReason(kDownloadStopReasonFailed);
    }

    // 最后做一个warn check，理论上任何流程分支下都应该设置了stop_reason_
    if (stop_reason_ == kDownloadStopReasonUnset) {
        LOG_WARN("[ScopeCurlHttpTask::DoDownload][line:%d]should not be here, curl_ret:%d, last_error:%d, stop_reason_:%d",
                 __LINE__, curl_ret, last_error_, stop_reason_);
    }
}

void ScopeCurlHttpTask::OnReceiveValidData(uint8_t* buf, int64_t len) {
    recv_valid_bytes_ += len;
    if (task_listener_) {
        task_listener_->OnReceiveData(buf, len);
    }
}

void ScopeCurlHttpTask::OnReceiveData(uint8_t* buf, int64_t income_len) {
    curl_off_t download_size = 0;
    curl_easy_getinfo(curl_, CURLINFO_SIZE_DOWNLOAD_T, &download_size);
    progress_helper_->OnProgress(download_size);

    if (server_not_support_range_) {
        int64_t remain_to_skip_bytes = to_skip_bytes_ - skipped_bytes_;
        if (remain_to_skip_bytes >= income_len) {
            // skip all bytes
            skipped_bytes_ += income_len;
            return;
        } else {
            skipped_bytes_ += remain_to_skip_bytes;
            auto valid_bytes = static_cast<int64_t>(income_len - (remain_to_skip_bytes));
            if (spec_.length > 0 && valid_bytes > spec_.length - recv_valid_bytes_) {
                // 不支持range请求的case，需要在满足了spec里的长度要求后就停止下载
                OnReceiveValidData(buf + remain_to_skip_bytes, spec_.length - recv_valid_bytes_);
                abort_ = true;
            } else {
                OnReceiveValidData(buf + remain_to_skip_bytes, valid_bytes);
            }
        }
    } else {
        OnReceiveValidData(buf, income_len);
    }

}

void* ScopeCurlHttpTask::DownloadThread(void* opaque) {
    auto task = static_cast<ScopeCurlHttpTask*>(opaque);
    AcUtils::SetThreadName("ScopeCurlHttpTask");
    task->DoDownload();
    return NULL;
}

size_t ScopeCurlHttpTask::HeaderCallback(char* buffer, size_t size, size_t nitems, ScopeCurlHttpTask* task) {
    task->http_header_.append(buffer, size * nitems);

    return size * nitems;
}

size_t ScopeCurlHttpTask::WriteCallback(char* buffer, size_t size, size_t nitems, ScopeCurlHttpTask* task) {
    // Handle make connection open.
    if (task->state_ == ScopeCurlHttpTask::State_Inited) {
        task->state_ = ScopeCurlHttpTask::State_Conneced;

        // CheckHeaders之前必须CollectResponseCode， 因为reponse code是CollectCurlInfo收集的
        task->CollectCurlInfo();

        int error = task->CheckResponseCode();
        if (error < 0) {
            return CURL_WRITEFUNC_PAUSE;
        }
        error = task->ParseCheckHeadersOnce();
        if (error < 0) {
            return CURL_WRITEFUNC_PAUSE;
        }
    }

    task->OnReceiveData((uint8_t*) buffer, size * nitems);

    return size * nitems;
}

int ScopeCurlHttpTask::ProgressCallback(ScopeCurlHttpTask* task,
                                        curl_off_t dltotal, curl_off_t dlnow,
                                        curl_off_t ultotal, curl_off_t ulnow) {

    if (task->last_error_ < 0) {
        return 1;
    }

    bool interrupted = task->IsInterrupted();
    if (task->abort_ || interrupted) {
        task->SetStopReason(kDownloadStopReasonCancelled);
        if (task->connection_info_.error_code != 0) {
            LOG_INFO("[%d][id:%d] [ScopeCurlHttpTask::ProgressCallback] task->abort_:%d, task->IsInterrupted() = %d,"
                     "error_code_:%d, stop_reason:%d \n",
                     task->context_id_, task->id(),
                     task->abort_, interrupted,
                     task->connection_info_.error_code, task->connection_info_.stop_reason);
        }
        // return non zero value to abort this call.
        return 1;
    }

    // to check if read timeout happend
    int64_t now = kpbase::SystemUtil::GetCPUTime();
    if (task->last_progress_callback_ts_ms_ == 0) {
        task->last_progress_callback_ts_ms_ = now;
        task->last_dlnow_ = dlnow;
    } else {
        if (task->last_dlnow_ >= dlnow) {
            // no progress
            int64_t time_diff_ms = now - task->last_progress_callback_ts_ms_;
            if (time_diff_ms >= task->options_.read_timeout_ms) {
                LOG_ERROR_DETAIL("[%d] [ScopeCurlHttpTask::ProgressCallback] id:%d, ProgressCallback timeout, task->last_dlnow_:%lld ,"
                                 " dlnow:%lld , time_diff_ms:%lldms \n",
                                 task->context_id_, task->id(), task->last_dlnow_, (int64_t)dlnow, time_diff_ms);
                task->SetErrorCode(kResultExceptionNetDataSourceReadTimeout);
                task->SetStopReason(kDownloadStopReasonTimeout);
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

int ScopeCurlHttpTask::SockOptCallback(void* clientp,
                                       curl_socket_t curlfd,
                                       curlsocktype purpose) {
    /* This return code was added in libcurl 7.21.5 */
    ScopeCurlHttpTask* task = static_cast<ScopeCurlHttpTask*>(clientp);
    int ret = 0;
    int recv_len = 0;
    socklen_t opt_len = sizeof(recv_len);

    ret = getsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &recv_len, &opt_len);
    if (ret < 0) {
        LOG_ERROR("[ScopeCurlHttpTask::SockOptCallback], getsockopt, original FAIL, ret:%d", ret);
    }
    if (task->ac_rt_info_) {
        task->ac_rt_info_->download_task.sock_orig_size_kb = recv_len > 0 ? recv_len / 1024 : recv_len;
    }

    if (purpose != CURLSOCKTYPE_IPCXN) {
        // 我们只设置CURLSOCKTYPE_IPCXN相关的连接
        LOG_ERROR("[ScopeCurlHttpTask::SockOptCallback], warning , purpose(%d) != CURLSOCKTYPE_IPCXN",
                  purpose);
        if (task->ac_rt_info_) {
            task->ac_rt_info_->download_task.sock_act_size_kb = -2; //暂时不知道线上有没有这种case，先用-2在灰度上区分一下数据
        }
        return CURL_SOCKOPT_OK;
    }

    if (kVerbose) {
        LOG_INFO("[%d] [ScopeCurlHttpTask::SockOptCallback]try to set socket buffer size:%dkb", task->context_id_,
                 task->options_.socket_buf_size_kb);
    }
    if (task->options_.socket_buf_size_kb > 0) {
        int set_recv_len = task->options_.socket_buf_size_kb * 1024;
        ret = setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &set_recv_len,
                         sizeof(set_recv_len));
        if (ret < 0) {
            LOG_ERROR("[ScopeCurlHttpTask::SockOptCallback], setsockopt FAIL, ret:%d", ret);
        }

        recv_len = -1;
        ret = getsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &recv_len, &opt_len);
        if (ret < 0) {
            LOG_ERROR("[ScopeCurlHttpTask::SockOptCallback], getsockopt after getsockopt FAIL, ret:%d", ret);
            if (task->ac_rt_info_) {
                task->ac_rt_info_->download_task.sock_act_size_kb =
                    task->ac_rt_info_->download_task.sock_orig_size_kb;
            }
        } else {
            if (task->ac_rt_info_) {
                task->ac_rt_info_->download_task.sock_act_size_kb =
                    recv_len > 0 ? recv_len / 1024 : recv_len;
            }
        }
    } else {
        if (task->ac_rt_info_) {
            task->ac_rt_info_->download_task.sock_act_size_kb =
                task->ac_rt_info_->download_task.sock_orig_size_kb;
        }
    }
    if (task->ac_rt_info_) {
        task->ac_rt_info_->download_task.sock_cfg_size_kb = task->options_.socket_buf_size_kb;
    }

    return CURL_SOCKOPT_OK;
}

int ScopeCurlHttpTask::ParseCheckHeadersOnce() {
    if (http_header_parsed_) {
        return last_error_;
    }

    http_header_parsed_ = true;
    int error = ParseHeader();
    if (error < 0) {
        SetErrorCode(error);
        SetStopReason(kDownloadStopReasonFailed);
        ReportInvalidResponseHeader();
        return error;
    }

    if (task_listener_) {
        task_listener_->OnConnectionInfoParsed(connection_info_);
    }

    return kResultOK;
}

void ScopeCurlHttpTask::ReportInvalidResponseHeader() {
    LOG_ERROR("[%d][ScopeCurlHttpTask::ReportInvalidResponseHeader] header:%s", context_id_, http_header_.c_str());
    if (ac_rt_info_) {
        ac_rt_info_->download_task.need_report_header = true;
        snprintf(ac_rt_info_->download_task.invalid_header, INVALID_RESPONSE_HEADER,
                 "%s", http_header_.c_str());
    }
}

int ScopeCurlHttpTask::ParseHeader() {
    std::string header = http_header_;
//    LOG_DEBUG("[%d][id:%d] [ScopeCurlHttpTask::ParseHeader] http_header:%s", context_id_, id(), http_header_.c_str());

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
        LOG_ERROR_DETAIL(
            "[%d][id:%d][ScopeCurlHttpTask::ParseHeader] fail, have not found any return_str \n",
            context_id_, id());
        ReportInvalidResponseHeader();
        return kCurlHttpResponseHeaderInvalid;
    }

    size_t header_position = 0;
    auto get_next_header_fn = [&]() {
        std::pair<std::string, std::string> res;
        while (true) {
            size_t next_position = header.find(return_str, header_position);
            if (next_position == -1) {
                return res;
            }
            std::string sub_header = header.substr(header_position, next_position - header_position);
            header_position = next_position + return_str_len;
            size_t colon_pos = sub_header.find(":");
            if (colon_pos != -1) {
                res.first = sub_header.substr(0, colon_pos);
                res.second = sub_header.substr(colon_pos + 1);
                return res;
            }
        }
        return res;
    };
    AcResultType ret = http::ParseResponseHeaders(
                           spec_, &connection_info_, &server_not_support_range_, &to_skip_bytes_,
                           get_next_header_fn);
    if (ac_rt_info_) {
        strncpy(ac_rt_info_->download_task.kwaisign, this->connection_info_.sign.c_str(), CDN_KWAI_SIGN_MAX_LEN);
        strncpy(ac_rt_info_->download_task.x_ks_cache, this->connection_info_.x_ks_cache.c_str(), CDN_X_KS_CACHE_MAX_LEN);
    }
    return ret;
}


AcResultType ScopeCurlHttpTask::CheckResponseCode() {
    // response_code strong check
    if (!connection_info_.IsResponseCodeSuccess()) {
        LOG_ERROR_DETAIL("[%d][id:%d][ScopeCurlHttpTask::CheckHeaders] connection_info_ error of resp code:%d, error_code = %d",
                         context_id_, id(), connection_info_.response_code, kCurlHttpResponseCodeFail);
        SetErrorCode(kHttpInvalidResponseCodeBase - connection_info_.response_code);
        SetStopReason(kDownloadStopReasonFailed);
        return kCurlHttpResponseCodeFail;
    }
    return kResultOK;
}


void ScopeCurlHttpTask::CollectCurlInfo() {
    if (curl_) {
        long response_code;
        long redirect_count;
        char* effective_url = nullptr;
        double dns_time;
        double connect_time;
        double start_transfer_time;
        char* ip_address;
        long http_version;

        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_getinfo(curl_, CURLINFO_REDIRECT_COUNT, &redirect_count);
        curl_easy_getinfo(curl_, CURLINFO_EFFECTIVE_URL, &effective_url);
        curl_easy_getinfo(curl_, CURLINFO_NAMELOOKUP_TIME, &dns_time);
        curl_easy_getinfo(curl_, CURLINFO_CONNECT_TIME, &connect_time);
        curl_easy_getinfo(curl_, CURLINFO_STARTTRANSFER_TIME, &start_transfer_time);
        curl_easy_getinfo(curl_, CURLINFO_PRIMARY_IP, &ip_address);
        curl_easy_getinfo(curl_, CURLINFO_HTTP_VERSION, &http_version);


        connection_info_.response_code = (int) response_code;
        connection_info_.connection_used_time_ms = static_cast<int>(connect_time * 1000);
        connection_info_.http_dns_analyze_ms = static_cast<int>(dns_time * 1000);
        connection_info_.http_first_data_ms = static_cast<int>(start_transfer_time * 1000);
        connection_info_.ip = ip_address ? ip_address : "";
        connection_info_.redirect_count = (int)redirect_count;
        connection_info_.effective_url = effective_url ? effective_url : "";

        if (ac_rt_info_) {
            ac_rt_info_->download_task.http_response_code = connection_info_.response_code;
            ac_rt_info_->download_task.http_connect_ms = connection_info_.connection_used_time_ms;
            ac_rt_info_->download_task.http_dns_analyze_ms = connection_info_.http_dns_analyze_ms;
            ac_rt_info_->download_task.http_first_data_ms = connection_info_.http_first_data_ms;
            snprintf(ac_rt_info_->download_task.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
                     connection_info_.ip.c_str());
            strncpy(ac_rt_info_->download_task.http_version,
                    http_version == CURL_HTTP_VERSION_1_1 ? "HTTP 1.1" :
                    http_version == CURL_HTTP_VERSION_1_0 ? "HTTP 1.0" : "HTTP UNKNOWN",
                    HTTP_VERSION_MAX_LEN);
            //针对hls多次连接，media playerlist index 1、2分别表示m3u8和ts
            // 暂时不支持hls，以后想好了怎么实现以及准备实现的时候再反注释这一段
//            if (ac_rt_info_->datasource_index <= CONNECT_INFO_COUNT) {
//                AwesomeCacheRuntimeInfo::ConnectInfo& ac_rt_connect_info = ac_rt_info_->connect_infos[ac_rt_info_->datasource_index - 1];
//                ac_rt_connect_info.position = spec_.position;
//                ac_rt_connect_info.length = spec_.length;
//                ScopeCurlHttpTask::CopyConnectionInfoToRuntimeInfo(ac_rt_connect_info, connection_info_);
//            }
        }

        if (kVerbose) {
            LOG_DEBUG("[%d][id:%d][ScopeCurlHttpTask::CollectCurlInfoOnce] connect_ms:%dms, dns_analyze:%dms, http_first_data:%dms, resp_code:%d",
                      context_id_, id(),
                      connection_info_.connection_used_time_ms,
                      connection_info_.http_dns_analyze_ms,
                      connection_info_.http_first_data_ms,
                      connection_info_.response_code);
        }
    }
}

bool ScopeCurlHttpTask::IsInterrupted() {
    return AwesomeCacheInterruptCB_is_interrupted(&interrupt_callback_);
}

void ScopeCurlHttpTask::WaitForTaskFinish() {
    if (!thread_joined) {
        int join_ret = pthread_join(thread_id_, NULL);
        if (join_ret != 0) {
            LOG_ERROR("[%d][id:%d][ScopeCurlHttpTask::WaitForTaskFinish], pthread_join fail, join_ret:%d",
                      context_id_, id(), join_ret);
        }
        thread_joined = true;
    }
}


void ScopeCurlHttpTask::SetErrorCode(int error_code) {
    last_error_ = error_code;
}

void ScopeCurlHttpTask::SetStopReason(DownloadStopReason reason) {
    stop_reason_ = reason;
}

void ScopeCurlHttpTask::CopyConnectionInfoToRuntimeInfo(
    AwesomeCacheRuntimeInfo::ConnectInfo& ac_rt_connect_info, ConnectionInfoV2& info) {
    ac_rt_connect_info.http_connect_ms = info.connection_used_time_ms;
    ac_rt_connect_info.http_dns_analyze_ms = info.http_dns_analyze_ms;
    ac_rt_connect_info.http_first_data_ms = info.http_first_data_ms;
    ac_rt_connect_info.first_data_ts = AcUtils::GetCurrentTime();
    snprintf(ac_rt_connect_info.resolved_ip, DATA_SOURCE_IP_MAX_LEN, "%s",
             info.ip.c_str());
}

void ScopeCurlHttpTask::Abort() {
    abort_ = true;
}


} // namespace cache
} // namespace kuaishou
