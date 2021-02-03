#ifdef CONFIG_VOD_P2SP

#include "./scope_p2sp_task.h"
#include "awesome_cache_interrupt_cb_c.h"

#include <unordered_set>
#include <unordered_map>
#include <set>

extern "C" {
#include <curl/curl.h>
}

#include "ksp2p_sdk.h"

using namespace kuaishou;
using namespace kuaishou::cache;

#define LOG_PREFIX "[ScopeP2spTask] "

#define STAT_INFO_STR_MAX_LEN (4096)
#define EXPECTED_CHUNK_SIZE (8192)

#define ROUND_CHUNK_SIZE_DOWN(X) ((X) / EXPECTED_CHUNK_SIZE * EXPECTED_CHUNK_SIZE)
#define ROUND_CHUNK_SIZE_UP(X) (((X) + EXPECTED_CHUNK_SIZE - 1) / EXPECTED_CHUNK_SIZE * EXPECTED_CHUNK_SIZE)

#define SDK_RESUBMIT_MIN_INTERVAL_BYTES (512 * 1024)

// error code of libcurl aborted, used for compatibility
static const int kResultUserInterrupt = (-2800 - CURLE_ABORTED_BY_CALLBACK);

// Comment following lines to enable debug log
#undef LOG_DEBUG
#define LOG_DEBUG(...)
// ----

namespace {
std::mutex sdk_inited_mtx;
bool sdk_inited = false;

void sdk_log_callback(int level, const char* content) {
    int ijk_level = IJK_LOG_INFO;
    switch (level) {
        case LOGLEVEL_ERROR:
            ijk_level = IJK_LOG_ERROR; break;
        case LOGLEVEL_WARN:
        case LOGLEVEL_SYSTEM:
            ijk_level = IJK_LOG_WARN; break;
        case LOGLEVEL_INFO:
        case LOGLEVEL_DEBUG:
            ijk_level = IJK_LOG_INFO; break;
        default:
            break;
    }
    ac_log(ijk_level, "KSP2P: %s", content);
}

void sdk_init_if_required(bool enable_log) {
    std::lock_guard<std::mutex> lock(sdk_inited_mtx);
    if (!sdk_inited) {
        ksp2p_init();
        char version[64] = {0};
        ksp2p_get_version(version, sizeof(version) / sizeof(version[0]));
        LOG_INFO(LOG_PREFIX "KSP2P Version: %s", version);
        sdk_inited = true;
    }
    if (enable_log) {
        ksp2p_set_log_func(&sdk_log_callback);
    }
}
}

namespace {
std::mutex p2sp_valid_pointers_mtx;
// remember all valid callback pointers
// so that callback-after-release is safe
std::unordered_map<size_t, ScopeP2spTask*> p2sp_valid_pointers;
size_t p2sp_valid_pointers_cnt = 0;

size_t p2sp_wrapper_register(ScopeP2spTask* p) {
    std::lock_guard<std::mutex> lock(p2sp_valid_pointers_mtx);
    size_t id = p2sp_valid_pointers_cnt++;
    p2sp_valid_pointers.emplace(id, p);
    return id;
}

void p2sp_wrapper_unregister(size_t id) {
    std::lock_guard<std::mutex> lock(p2sp_valid_pointers_mtx);
    p2sp_valid_pointers.erase(id);
}

int p2sp_data_callback_wrapper(void* p, uint64_t offset, uint8_t const* data, uint64_t len, uint32_t check_code) {
    std::lock_guard<std::mutex> lock(p2sp_valid_pointers_mtx);
    auto it = p2sp_valid_pointers.find(reinterpret_cast<size_t>(p));
    if (it == p2sp_valid_pointers.end()) {
        return 0;
    }
    return it->second->p2sp_data_callback(offset, data, len);
}

int p2sp_timeout_callback_wrapper(int64_t offset, uint32_t len, void* p) {
    std::lock_guard<std::mutex> lock(p2sp_valid_pointers_mtx);
    auto it = p2sp_valid_pointers.find(reinterpret_cast<size_t>(p));
    if (it == p2sp_valid_pointers.end()) {
        return 0;
    }
    return it->second->p2sp_timeout_callback(offset, len);
}
int p2sp_error_callback_wrapper(void* p, int code, char const* msg) {
    std::lock_guard<std::mutex> lock(p2sp_valid_pointers_mtx);
    auto it = p2sp_valid_pointers.find(reinterpret_cast<size_t>(p));
    if (it == p2sp_valid_pointers.end()) {
        return 0;
    }
    return it->second->p2sp_error_callback(code, msg);
}

}

ScopeP2spTask::ScopeP2spTask(DownloadOpts const& opts,
                             ScopeTaskListener* listener,
                             AwesomeCacheRuntimeInfo* ac_rt_info):
    opts_(opts),
    listener_(listener),
    ac_rt_info_(ac_rt_info),
    cdn_listener_proxy_(new ScopeP2spTaskCdnListenerProxy(this)) {
    memset(&ac_rt_info_->vod_p2sp, 0, sizeof(ac_rt_info_->vod_p2sp));
    ac_rt_info_->vod_p2sp.enabled = true;
    ksp2p_get_version(this->ac_rt_info_->vod_p2sp.p2sp_sdk_version,
                      sizeof(this->ac_rt_info_->vod_p2sp.p2sp_sdk_version) / sizeof(this->ac_rt_info_->vod_p2sp.p2sp_sdk_version[0]));
    this->p2sp_wrapper_id_ = p2sp_wrapper_register(this);
    if (this->opts_.player_statistic) {
        this->player_statistic_it_ = this->opts_.player_statistic->add_listener(
                                         std::bind(&ScopeP2spTask::update_player_statistic, this, std::placeholders::_1));
    }
}

ScopeP2spTask::~ScopeP2spTask() {
    if (!this->p2sp_url_.empty()) {
        LOG_INFO(LOG_PREFIX "Destroying P2sp task %s", this->p2sp_url_.c_str());
        ksp2p_stop_task(this->p2sp_url_.c_str());
        ksp2p_destroy_task(this->p2sp_url_.c_str());
    }
    p2sp_wrapper_unregister(this->p2sp_wrapper_id_);
    if (this->opts_.player_statistic) {
        this->opts_.player_statistic->remove_listener(this->player_statistic_it_);
    }
}

int64_t ScopeP2spTask::Open(DataSpec const& spec) {
    std::unique_lock<std::mutex> lock(this->mtx_);
    this->dataspec_orig_ = spec;
    this->transfer_start_timestamp_ = kpbase::SystemUtil::GetCPUTime();

    LOG_INFO(LOG_PREFIX "Open: spec.position=%lld, spec.length=%lld, p2sp_start=%lld, p2sp_end=%lld",
             spec.position, spec.length,
             this->p2sp_datasource_submitted_start_,
             this->p2sp_datasource_submitted_end_);

    this->data_start_ = spec.position;
    this->current_read_pos_ = this->data_start_;

    if (this->file_length_ < 0) {
        if (spec.length != kLengthUnset) {
            this->data_end_ = spec.position + spec.length;
        }
        // data_start, data_end, current_read_pos would be updated on first cdn open callback
        // open initial cdn request
        LOG_INFO(LOG_PREFIX "Open: file_length not set yet, open initial cdn datasource");
        lock.unlock();
        auto ret = this->cdn_datasource_open(-1);
        if (ret < 0) {
            LOG_ERROR(LOG_PREFIX "Open cdn datasource failed: %lld", ret);
            return ret;
        }
    } else {
        this->data_end_ = this->file_length_;
        if (spec.length != kLengthUnset) {
            this->data_end_ = std::min(this->data_end_, spec.position + spec.length);
        }
        // connection info callback
        this->connection_info_.content_length = this->data_end_ - this->data_start_;
        this->connection_info_.connection_used_time_ms = 0;
        this->connection_info_.http_dns_analyze_ms = 0;
        this->connection_info_.http_first_data_ms = 0;
        this->connection_info_.redirect_count = 0;
        this->connection_info_.downloaded_bytes_from_curl = 0;
        this->connection_info_.error_code = 0;
        this->connection_info_.stop_reason = kDownloadStopReasonUnknown;
        this->connection_info_.transfer_consume_ms = 0;
        LOG_INFO(LOG_PREFIX "Open: file_length already set, callback immediantly");
        this->listener_->OnConnectionInfoParsed(this->connection_info_);
    }

    this->runloop_abort_ = false;
    this->runloop_ = std::thread([this]() {
        while (!this->runloop_abort_ && !AwesomeCacheInterruptCB_is_interrupted(&this->opts_.interrupt_cb)) {
            std::unique_lock<std::mutex> lock(this->mtx_);
            this->runloop_cond_.wait_for(lock, std::chrono::seconds(1));
            this->p2sp_submit_if_required();
            this->try_emit_p2sp_data(lock);
        }
        std::unique_lock<std::mutex> lock(this->mtx_);
        if (AwesomeCacheInterruptCB_is_interrupted(&this->opts_.interrupt_cb) || this->current_read_pos_ < this->data_end_)
            this->on_download_complete(kResultUserInterrupt, kDownloadStopReasonCancelled);
    });

    return 0;
}

void ScopeP2spTask::Close() {
    {
        std::lock_guard<std::mutex> lock(this->mtx_);
        LOG_INFO(LOG_PREFIX "Closing, current=%lld, cdn start=%lld, cdn end=%lld, p2sp start=%lld, p2sp end=%lld",
                 this->current_read_pos_, this->cdn_submitted_start_, this->cdn_submitted_end_,
                 this->p2sp_datasource_submitted_start_,
                 this->p2sp_datasource_submitted_end_);

        this->data_start_ = this->data_end_ = 0;
        this->collect_sdk_details(true);
    }

    this->cdn_datasource_close();

    {
        std::lock_guard<std::mutex> lock(this->mtx_);
        this->runloop_abort_ = true;
        this->runloop_cond_.notify_all();
    }
    if (this->runloop_.joinable()) {
        this->runloop_.join();
    }
}

void ScopeP2spTask::Abort() {
    // FIXME
    this->Close();
}

void ScopeP2spTask::WaitForTaskFinish() {
    LOG_INFO(LOG_PREFIX "WaitForTaskFinish");
    std::shared_ptr<ScopeCurlHttpTask> cdn_task = nullptr;
    {
        std::lock_guard<std::mutex> lock(this->mtx_);
        std::swap(cdn_task, this->cdn_task_);
    }
    if (cdn_task) {
        LOG_INFO(LOG_PREFIX "Waiting cdn task...");
        cdn_task->WaitForTaskFinish();
        LOG_INFO(LOG_PREFIX "Waiting cdn task...done");
    }
    std::lock_guard<std::mutex> lock(this->mtx_);
    this->cdn_submitted_start_ = this->cdn_submitted_end_ = 0;
}

void ScopeP2spTask::cdn_datasource_close() {
    std::shared_ptr<ScopeCurlHttpTask> cdn_task = nullptr;
    {
        std::lock_guard<std::mutex> lock(this->mtx_);
        std::swap(cdn_task, this->cdn_task_);
        this->cdn_submitted_start_ = this->cdn_submitted_end_ = 0;
    }
    if (cdn_task) {
        LOG_INFO(LOG_PREFIX "Closing cdn task...");
        cdn_task->Close();
        LOG_INFO(LOG_PREFIX "Closing cdn task...done");
    }
}

int64_t ScopeP2spTask::cdn_datasource_open(int64_t next_valid_data) {
    this->cdn_datasource_close();

    std::lock_guard<std::mutex> lock(this->mtx_);
    this->ac_rt_info_->vod_p2sp.cdn_open_count += 1;
    DataSpec spec_cdn = this->dataspec_orig_;
    spec_cdn.position = this->current_read_pos_;
    // length
    spec_cdn.length = this->opts_.vod_p2sp_cdn_request_max_size;
    LOG_INFO(LOG_PREFIX "cdn_datasource_open: cdn request max size: %lld",
             this->opts_.vod_p2sp_cdn_request_max_size);
    if (this->p2sp_datasource_submitted_start_ == this->p2sp_datasource_submitted_end_) {
        spec_cdn.length = this->opts_.vod_p2sp_cdn_request_initial_size;
        LOG_INFO(LOG_PREFIX "cdn_datasource_open: p2sp not yet started, use initial max size: %lld",
                 spec_cdn.length);
    }
    if (this->p2sp_error_code_ != 0 || this->p2sp_url_.empty()) {
        spec_cdn.length = std::numeric_limits<int64_t>::max();
        LOG_INFO(LOG_PREFIX "cdn_datasource_open: p2sp error occured or disabled, use max request length");
    }

    if (this->p2sp_datasource_submitted_start_ - this->current_read_pos_ > spec_cdn.length) {
        spec_cdn.length = this->p2sp_datasource_submitted_start_ - this->current_read_pos_;
        LOG_INFO(LOG_PREFIX "cdn_datasource_open: p2sp submitted in %lld, extend length to %lld",
                 this->p2sp_datasource_submitted_start_, spec_cdn.length);
    }
    if (this->data_end_ > 0 && this->data_end_ - this->current_read_pos_ < spec_cdn.length) {
        spec_cdn.length = this->data_end_ - this->current_read_pos_;
        LOG_INFO(LOG_PREFIX "cdn_datasource_open: data end is %lld, current is %lld, trim request length to %lld",
                 this->data_end_, this->current_read_pos_, spec_cdn.length);
    }
    if (next_valid_data >= 0 && next_valid_data - this->current_read_pos_ > spec_cdn.length) {
        spec_cdn.length = next_valid_data - this->current_read_pos_;
        LOG_INFO(LOG_PREFIX "cdn_datasource_open: next valid data is at %lld, current is %lld, trim request length to %lld",
                 next_valid_data, this->current_read_pos_, spec_cdn.length);
    }
    if (spec_cdn.length == std::numeric_limits<int64_t>::max()) {
        spec_cdn.length = kLengthUnset;
    }

    this->cdn_submitted_start_ = spec_cdn.position;
    this->cdn_submitted_end_ = spec_cdn.position + spec_cdn.length;

    this->cdn_task_.reset(new ScopeCurlHttpTask(this->opts_, this->cdn_listener_proxy_.get(), this->ac_rt_info_));
    return this->cdn_task_->Open(spec_cdn);
}

int64_t ScopeP2spTask::get_current_player_buffer_ms(bool includes_cached) {
    if (this->player_pre_read_ms_ == 0 || this->player_last_updated_ == 0) {
        return 0;
    }
    int64_t now = kpbase::SystemUtil::GetCPUTime();
    int64_t res = this->player_buffer_ms_ - (now - this->player_last_updated_);

    if (includes_cached && this->player_bitrate_ > 0) {
        // TODO: cached_bytes is at most 1M (scope size) for now
        // Should we also consider those cached bytes in p2sp_chunks_?
        // But they stay in memory. We should write them to scope cache.
        int64_t cached_bytes = this->current_read_pos_ - this->player_read_pos_;
        cached_bytes = std::max((int64_t)0, cached_bytes);
        res += (cached_bytes * 8 * 1000 / this->player_bitrate_);
    }

    res = std::max((int64_t)0, res);
    this->ac_rt_info_->vod_p2sp.player_buffer_ms = res;
    return res;
}

bool ScopeP2spTask::is_buffer_below_p2sp_off_threshold() {
    if (this->player_pre_read_ms_ == 0 || this->player_last_updated_ == 0) {
        return true;
    }
    auto current_player_buffer_ms = this->get_current_player_buffer_ms(true);
    // mode 0: threshold means percentage
    if (this->opts_.vod_p2sp_off_threshold >= 0)
        return (100 * current_player_buffer_ms / this->player_pre_read_ms_) < this->opts_.vod_p2sp_off_threshold;

    // mode 1: threshold means absolute time in ms
    return current_player_buffer_ms < std::abs(this->opts_.vod_p2sp_off_threshold) && current_player_buffer_ms < player_pre_read_ms_;
}

bool ScopeP2spTask::is_buffer_above_p2sp_on_threshold() {
    if (this->player_pre_read_ms_ == 0 || this->player_last_updated_ == 0) {
        return false;
    }
    auto current_player_buffer_ms = this->get_current_player_buffer_ms(true);
    // mode 0: threshold means percentage
    if (this->opts_.vod_p2sp_on_threshold >= 0)
        return (100 * current_player_buffer_ms / this->player_pre_read_ms_) > this->opts_.vod_p2sp_on_threshold;

    // mode 1: threshold means absolute time in ms
    return current_player_buffer_ms > std::abs(this->opts_.vod_p2sp_on_threshold) || current_player_buffer_ms > player_pre_read_ms_;
}

void ScopeP2spTask::try_emit_p2sp_data(std::unique_lock<std::mutex>& lock) {
    // std::unique_lock<std::mutex> lock(this->mtx_);

    if (this->current_read_pos_ >= this->data_end_)
        return;

    // If the cdn task is already opened, we can just wait for it
    if (this->current_read_pos_ >= this->cdn_submitted_start_ && this->current_read_pos_ < this->cdn_submitted_end_)
        return;

    // emit p2sp chunks as many as we want
    // Try to find `it` in the following block of code
    auto it = this->p2sp_chunks_.end();
    if (!this->p2sp_chunks_.empty()) {
        auto it_next = this->p2sp_chunks_.upper_bound(this->current_read_pos_);
        if (it_next != this->p2sp_chunks_.begin()) {
            it = it_next;
            it --;
        }
    }
    // Loop, as long as `it` contains valid data
    // After this, either we return (continue waiting), or open a new cdn task
    for (;
         it != this->p2sp_chunks_.end() &&
         it->first <= this->current_read_pos_ &&
         it->first + it->second.size() > this->current_read_pos_;
         it++
        ) {
        int64_t chunk_read_offset = this->current_read_pos_ - it->first;
        int64_t chunk_read_len = std::min((int64_t)it->second.size() - chunk_read_offset,
                                          this->data_end_ - this->current_read_pos_);
        assert(chunk_read_len > 0);

        LOG_DEBUG(LOG_PREFIX "Reading from p2sp chunk: chunk start=%lld, chunk size=%d, read offset=%lld, read len=%lld, current=%lld",
                  it->first, it->second.size(), chunk_read_offset, chunk_read_len, this->current_read_pos_);

        this->current_read_pos_ += chunk_read_len;
        this->ac_rt_info_->vod_p2sp.p2sp_bytes_used += chunk_read_len;
        this->on_receive_data(it->second.data() + chunk_read_offset, chunk_read_len);

        if (this->current_read_pos_ == this->data_end_) {
            LOG_DEBUG(LOG_PREFIX "Reading reached data_end, return");
            this->on_download_complete(0, kDownloadStopReasonFinished);
            return;
        }
    }
    // now, `it` is the next valid chunk (or end())
    // and there's a hold between current_read_pos_ and `it`
    // should we keep waiting?
    if (this->current_read_pos_ >= this->p2sp_datasource_submitted_start_ &&
        this->current_read_pos_ < this->p2sp_datasource_submitted_end_ &&
        this->p2sp_error_code_ == 0 &&
        !this->is_buffer_below_p2sp_off_threshold()) {
        LOG_DEBUG(LOG_PREFIX "Buffer percentage is higher than threshold and we're waiting for p2sp");
        return;
    }

    // ok.. now it's serious, we need to open a critical cdn task
    LOG_DEBUG(LOG_PREFIX "Opening cdn datasource for hole chunk (also stop p2sp task)");

    // Stop p2sp task
    if (!this->p2sp_url_.empty() &&
        this->p2sp_datasource_submitted_end_ > this->p2sp_datasource_submitted_start_) {
        ksp2p_stop_task(this->p2sp_url_.c_str());
        this->p2sp_datasource_submitted_start_ = 0;
        this->p2sp_datasource_submitted_end_ = 0;
        this->ac_rt_info_->vod_p2sp.on = false;
    }

    int next_valid = it == this->p2sp_chunks_.end() ? -1 : it->first;
    lock.unlock();

    int64_t ret = this->cdn_datasource_open(next_valid);
    if (ret < 0) {
        LOG_ERROR(LOG_PREFIX "Open cdn datasource for hole chunk failed: %lld", ret);
        lock.lock();
        this->cdn_finish_callback(ret, kDownloadStopReasonFailed);
    }
}

void ScopeP2spTask::cdn_connected_callback(ConnectionInfoV2 const& info) {
    std::lock_guard<std::mutex> lock(this->mtx_);

    if (this->file_length_ >= 0) {
        LOG_INFO(LOG_PREFIX "Cdn task connected. It's for the hole chunk. Ignore Connection info");
        return;
    }

    LOG_INFO(LOG_PREFIX "Initial cdn task connected. Setup everything");
    this->connection_info_ = info;
    this->file_length_ = info.GetFileLength();
    this->data_start_ = this->dataspec_orig_.position;
    this->data_end_ = this->file_length_;
    if (this->dataspec_orig_.length != kLengthUnset)
        this->data_end_ = std::min(this->data_end_, this->data_start_ + this->dataspec_orig_.length);
    this->current_read_pos_ = this->data_start_;
    this->ac_rt_info_->vod_p2sp.file_length = file_length_;

    LOG_INFO(LOG_PREFIX "Original request: [%lld, %lld), cdn request: [%lld, %lld), filesize=%lld",
             this->data_start_, this->data_end_,
             this->cdn_submitted_start_, this->cdn_submitted_end_,
             this->file_length_);
    this->listener_->OnConnectionInfoParsed(info);

    // Prepare p2sp
    std::string url_host = info.http_dns_host;
    auto url_host_begin = info.uri.find("://");
    if (url_host_begin != std::string::npos) {
        url_host_begin += 3;
    }
    auto url_host_end = url_host_begin == std::string::npos ? std::string::npos : info.uri.find("/", url_host_begin);
    if (url_host.empty() && url_host_begin != std::string::npos && url_host_end != std::string::npos) {
        url_host = info.uri.substr(url_host_begin, url_host_end - url_host_begin);
    }

    if (this->p2sp_url_.empty()) {
        // init p2sp, but do not start receiving yet

        // translate uri, replace ip address with host
        this->p2sp_url_ = info.uri;
        if (url_host_begin != std::string::npos && url_host_end != std::string::npos) {
            this->p2sp_url_ = info.uri.substr(0, url_host_begin) + url_host + info.uri.substr(url_host_end);
        }

        LOG_INFO(LOG_PREFIX "Initializing P2sp task %s", this->p2sp_url_.c_str());

        bool enable_log = std::abs(this->opts_.vod_p2sp_on_threshold) % 2 == 1; // FIXME: quick hack, for debugging
        sdk_init_if_required(enable_log);
        int open_task_ret = ksp2p_create_task(this->p2sp_url_.c_str(), TASK_MODE_P2P);

        if (open_task_ret != 0) {
            LOG_WARN(LOG_PREFIX "Error opening p2sp task, reset p2sp_url");
            this->p2sp_url_.clear();
        }
    }
}

void ScopeP2spTask::cdn_data_callback(uint8_t* data, int64_t data_len) {
    std::lock_guard<std::mutex> lock(this->mtx_);

    LOG_DEBUG(LOG_PREFIX "Cdn data callback, len=%lld, current=%lld", data_len, this->current_read_pos_);
    if (this->current_read_pos_ < this->cdn_submitted_start_ ||
        this->current_read_pos_ >= this->cdn_submitted_end_) {
        LOG_ERROR(LOG_PREFIX "Invalid data, cdn submitted = [%lld, %lld)",
                  this->cdn_submitted_start_, this->cdn_submitted_end_);
        return;
    }

    // forward to listener
    this->current_read_pos_ += data_len;
    this->ac_rt_info_->vod_p2sp.cdn_bytes += data_len;
    this->on_receive_data(data, data_len);

    if (this->current_read_pos_ >= this->data_end_) {
        LOG_DEBUG(LOG_PREFIX "Reading reached data_end, emit DownloadComplete");
        this->on_download_complete(0, kDownloadStopReasonFinished);
    }
}

void ScopeP2spTask::cdn_finish_callback(int32_t err, int32_t stop_reason) {
    std::lock_guard<std::mutex> lock(this->mtx_);
    if (err == 0) {
        LOG_DEBUG(LOG_PREFIX "Cdn finished without error");
        return;
    }

    if (this->current_read_pos_ < this->cdn_submitted_start_ ||
        this->current_read_pos_ >= this->cdn_submitted_end_) {
        LOG_WARN(LOG_PREFIX "Cdn finished with error %d, %d, but not withing range", err, stop_reason);
        return;
    }

    LOG_WARN(LOG_PREFIX "Cdn finished with err %d, %d", err, stop_reason);
    this->current_read_pos_ = this->data_end_;
    this->on_download_complete(err, (DownloadStopReason)stop_reason);
    this->p2sp_submit_if_required();
    return;
}

int ScopeP2spTask::p2sp_data_callback(int64_t offset, uint8_t const* data, uint32_t len) {
    if (len <= 0 || !data) {
        return 0;
    }

    std::unique_lock<std::mutex> lock(this->mtx_);
    LOG_DEBUG(LOG_PREFIX "Got P2SP data: offset=%lld, len=%d", offset, len);
    this->p2sp_clean_chunks();

    if (this->p2sp_chunks_bytes_ > this->p2sp_chunks_max_bytes_) {
        LOG_WARN(LOG_PREFIX "P2sp chunks full, ignore new data");
        return 0;
    }

    std::vector<uint8_t> data_vec(data, data + len);
    auto emplace_ret = this->p2sp_chunks_.emplace(offset, std::move(data_vec));
    if (emplace_ret.second) {
        this->p2sp_chunks_bytes_ += len;
    }

    this->ac_rt_info_->vod_p2sp.p2sp_bytes_received += len;
    if (this->ac_rt_info_->vod_p2sp.p2sp_first_byte_offset == -1) {
        this->ac_rt_info_->vod_p2sp.p2sp_first_byte_offset = this->current_read_pos_;
    }
    if (this->ac_rt_info_->vod_p2sp.p2sp_first_byte_duration == -1) {
        this->ac_rt_info_->vod_p2sp.p2sp_first_byte_duration = kpbase::SystemUtil::GetCPUTime() - this->p2sp_datasource_submitted_timestamp_;
    }

    //calculate repeated p2sp bytes
    int64_t p2sp_start = offset;
    int64_t p2sp_end = offset + len;
    int64_t min_end = (this->cdn_submitted_end_ <= p2sp_end) ? this->cdn_submitted_end_ : p2sp_end;
    int64_t repeated_len = (min_end > p2sp_start) ? (min_end - p2sp_start) : 0;
    this->ac_rt_info_->vod_p2sp.p2sp_bytes_repeated += repeated_len;

    this->collect_sdk_details();

    this->runloop_cond_.notify_one();
    return 0;
}

int ScopeP2spTask::p2sp_timeout_callback(int64_t offset, uint32_t len) {
    LOG_ERROR(LOG_PREFIX "P2sp timeout callback: %ld - %d", offset, len);
    std::unique_lock<std::mutex> lock(this->mtx_);
    this->collect_sdk_details(true);
    return 0;
}

int ScopeP2spTask::p2sp_error_callback(int code, char const* msg) {
    LOG_ERROR(LOG_PREFIX "P2sp error callback: %d - %s", code, msg);

    std::unique_lock<std::mutex> lock(this->mtx_);
    this->p2sp_error_code_ = code;
    this->ac_rt_info_->vod_p2sp.p2sp_error_code = code;
    this->collect_sdk_details(true);
    this->runloop_cond_.notify_one();
    return 0;
}

void ScopeP2spTask::update_player_statistic(PlayerStatistic const* statistic) {
    std::unique_lock<std::mutex> lock(this->mtx_);
    this->player_bitrate_ = statistic->bitrate;
    this->player_pre_read_ms_ = statistic->pre_read_ms;
    this->player_read_pos_ = statistic->read_position_bytes;
    this->player_buffer_ms_ = std::min(statistic->audio_cache_duration_ms, statistic->video_cache_duration_ms);
    this->player_last_updated_ = kpbase::SystemUtil::GetCPUTime();

    LOG_DEBUG(LOG_PREFIX "Got player statistic: buffer=%lld/%lld, pre_read=%lld, bitrate=%lld, read_pos=%lld",
              statistic->audio_cache_duration_ms, statistic->video_cache_duration_ms, this->player_pre_read_ms_, this->player_bitrate_, this->player_read_pos_);

    this->p2sp_submit_if_required();
    this->runloop_cond_.notify_one();
}

void ScopeP2spTask::p2sp_submit_if_required() {
    if (this->p2sp_url_.empty() || this->file_length_ <= 0 || this->p2sp_error_code_ != 0) {
        return;
    }

    if (!(this->is_buffer_above_p2sp_on_threshold() ||
          (this->p2sp_datasource_submitted_end_ > this->p2sp_datasource_submitted_start_ &&  // pcdn already open
           !this->is_buffer_below_p2sp_off_threshold()))) {
        return;
    }

    if (this->current_read_pos_ >= this->file_length_) {
        return;
    }

    if (this->p2sp_chunks_max_bytes_ == 0) {
        if (this->opts_.vod_p2sp_task_max_size >= 0) {
            // mode 0: max_size is in bytes
            this->p2sp_chunks_max_bytes_ = this->opts_.vod_p2sp_task_max_size;
        } else {
            // mode 1: max_size is in ms, consider bitrate
            this->p2sp_chunks_max_bytes_ = std::abs(this->opts_.vod_p2sp_task_max_size) * this->player_bitrate_ / 1000 / 8;
        }
    }

    if (this->p2sp_chunks_bytes_ > this->p2sp_chunks_max_bytes_) {
        this->p2sp_clean_chunks();
        if (this->p2sp_chunks_bytes_ > this->p2sp_chunks_max_bytes_) {
            LOG_WARN(LOG_PREFIX "P2sp chunks full, do not start");
            return;
        }
    }

    int64_t task_start = ROUND_CHUNK_SIZE_DOWN(this->current_read_pos_);
    int64_t task_end = ROUND_CHUNK_SIZE_UP(task_start + this->p2sp_chunks_max_bytes_);
    task_end = std::min(task_end, this->file_length_);

    if (task_start < task_end &&
        task_start >= this->cdn_submitted_end_ &&  // never overlap with cdn datasource
        (this->p2sp_datasource_submitted_end_ == 0 ||   // p2sp datasource has never been submitted, or..
         std::abs(task_end - this->p2sp_datasource_submitted_end_) > SDK_RESUBMIT_MIN_INTERVAL_BYTES)  // re-submit
       ) {
        LOG_INFO(LOG_PREFIX "(re-)Submitting P2SP task %s: [%lld, %lld)",
                 this->p2sp_url_.c_str(), task_start, task_end);

        struct task_info task;
        task.caller = reinterpret_cast<void*>(this->p2sp_wrapper_id_);
        task.filesize = this->file_length_;
        task.start = task_start;
        task.end = task_end - 1;
        task.timeout = this->opts_.vod_p2sp_task_timeout;
        task.recv_cb = p2sp_data_callback_wrapper;
        task.error_cb = p2sp_error_callback_wrapper;
        int ret = ksp2p_get_data(this->p2sp_url_.c_str(), &task);

        LOG_INFO(LOG_PREFIX "Submitted P2SP task: %d", ret);
#if 1
        //requested = range sub last request range
        int64_t requested = task_end - task_start;
        int64_t repeated = 0;
        if (this->ac_rt_info_->vod_p2sp.p2sp_last_end > 0) {
            repeated = (std::min(task_end, this->ac_rt_info_->vod_p2sp.p2sp_last_end) - std::max(task_start, this->ac_rt_info_->vod_p2sp.p2sp_last_start));
        }
        if (repeated > 0) {
            requested -= repeated;
        }
        this->ac_rt_info_->vod_p2sp.p2sp_bytes_requested += requested;
#else
        /*
        p2sp_last_start    p2sp_last_end   上次P2P提交起始位置
            |____________________|
                                            start                      end    本次P2P提交起始位置
                                              |_________________________|
        */
        if (task_start >= this->ac_rt_info_->vod_p2sp.p2sp_last_end) {
            this->ac_rt_info_->vod_p2sp.p2sp_bytes_requested += task_end - task_start;
        }
        /*
        p2sp_last_start    p2sp_last_end    上次P2P提交起始位置
            |____________________|
                   start                              end    本次P2P提交起始位置
                     |_________________________________|
        */
        else if (task_start >= this->ac_rt_info_->vod_p2sp.p2sp_last_start &&
                 task_start < this->ac_rt_info_->vod_p2sp.p2sp_last_end &&
                 task_end > this->ac_rt_info_->vod_p2sp.p2sp_last_end) {
            this->ac_rt_info_->vod_p2sp.p2sp_bytes_requested += (task_end - this->ac_rt_info_->vod_p2sp.p2sp_last_end);
        }
#endif
        this->ac_rt_info_->vod_p2sp.p2sp_last_start = task_start;
        this->ac_rt_info_->vod_p2sp.p2sp_last_end = task_end;
        this->ac_rt_info_->vod_p2sp.on = true;

        this->p2sp_datasource_submitted_start_ = task_start;
        this->p2sp_datasource_submitted_end_ = task_end;
        int p2sp_open_count = this->ac_rt_info_->vod_p2sp.p2sp_open_count++;
        if (p2sp_open_count == 0) {
            this->p2sp_datasource_submitted_timestamp_ = kpbase::SystemUtil::GetCPUTime();
            this->ac_rt_info_->vod_p2sp.p2sp_start = task_start;
            this->ac_rt_info_->vod_p2sp.p2sp_first_byte_offset = -1;
            this->ac_rt_info_->vod_p2sp.p2sp_first_byte_duration = -1;
        }

        this->collect_sdk_details(true);
    }
}

void ScopeP2spTask::p2sp_clean_chunks() {
    int current_read_pos = this->current_read_pos_;

    // erase old chunks
    auto it_max = this->p2sp_chunks_.lower_bound(current_read_pos_);
    if (it_max != this->p2sp_chunks_.begin()) {
        auto it = it_max;
        it -- ;
        if (it->first + it->second.size() > current_read_pos_) {
            // cannot erase *it
            it_max = it;
        }
    }

    int erase_size = 0;
    for (auto it = this->p2sp_chunks_.begin() ; it != it_max ; it ++) {
        this->p2sp_chunks_bytes_ -= it->second.size();
        erase_size += 1;
    }
    this->p2sp_chunks_.erase(this->p2sp_chunks_.begin(), it_max);
    LOG_DEBUG(LOG_PREFIX "Erased %d p2sp chunks, current read pos=%lld, remain chunks=%d (bytes %lld), next chunk start=%lld",
              erase_size, current_read_pos_,
              this->p2sp_chunks_.size(),
              this->p2sp_chunks_bytes_,
              it_max == this->p2sp_chunks_.end() ? -1 : it_max->first);
}


void ScopeP2spTask::on_receive_data(uint8_t* data, int64_t data_len) {
    this->connection_info_.downloaded_bytes_from_curl += data_len;
    if (this->listener_) {
        this->listener_->OnReceiveData(data, data_len);
    }
}

void ScopeP2spTask::on_download_complete(int err, DownloadStopReason stop_reason) {
    this->connection_info_.error_code = err;
    this->connection_info_.stop_reason = stop_reason;
    if (stop_reason == kDownloadStopReasonFinished) {
        this->connection_info_.transfer_consume_ms = kpbase::SystemUtil::GetCPUTime() - this->transfer_start_timestamp_;
    }
    if (this->listener_) {
        this->listener_->OnDownloadComplete(err, stop_reason);
    }
}

void ScopeP2spTask::collect_sdk_details(bool force) {
    auto now = std::chrono::steady_clock::now();
    if (!force && std::chrono::duration_cast<std::chrono::milliseconds>(
            now - this->collect_sdk_details_last_time).count() < 1000)
        return;
    this->collect_sdk_details_last_time = now;

    if (!this->p2sp_url_.empty()) {
        std::vector<char> task_stat(STAT_INFO_STR_MAX_LEN, '\0');
        if (ksp2p_get_task_stat(this->p2sp_url_.c_str(), task_stat.data(), STAT_INFO_STR_MAX_LEN - 1) >= 0) {
            if (this->ac_rt_info_->vod_p2sp.sdk_details) {
                char* oldp = this->ac_rt_info_->vod_p2sp.sdk_details;
                this->ac_rt_info_->vod_p2sp.sdk_details = nullptr;
                free(oldp);
            }
            this->ac_rt_info_->vod_p2sp.sdk_details = strdup(task_stat.data());
        }
    }
}

#endif
