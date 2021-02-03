#ifdef CONFIG_LIVE_P2SP

#include <chrono>
#include <algorithm>

#include "./live_xyp2sp_http_data_source.h"

#include "./live_xyp2sp_sdk_helper.h"
#include "./ffurl_http_data_source.h"

// 关闭本文件中的DEBUG日志
#undef LOG_DEBUG
#define LOG_DEBUG(...)
// ----

using namespace kuaishou;
using namespace kuaishou::cache;

namespace {

class FlvTagDataChunk: public LiveXyP2spHttpDataSource::DataChunk {
  protected:
    virtual int getTagType() = 0;
  public:
    tag_hash_t getHash() override {
        return
            (this->getTimestamp() & 0xffffffffULL) |
            ((uint64_t)this->getSize() & 0xffffffULL) << 32 |
            ((uint64_t)this->getTagType() & 0xff) << 56;
    }
};

class XyFlvTagDataChunk: public FlvTagDataChunk {
  private:
    xy_flv_tag* tag_ = nullptr;
  public:
    XyFlvTagDataChunk(XyFlvTagDataChunk& another) = delete;
    XyFlvTagDataChunk& operator = (XyFlvTagDataChunk& another) = delete;

    XyFlvTagDataChunk(xy_flv_tag* tag = nullptr): tag_(tag) {}
    ~XyFlvTagDataChunk() {
        if (tag_)
            xy_flv_tag_delete(tag_);
    }

    uint8_t* getData() override {
        return tag_ ? tag_->tag : nullptr;
    }

    uint32_t getSize() override {
        return tag_ ? tag_->tagSize : 0;
    }

    int64_t getTimestamp() override {
        return tag_ ? tag_->timestamp : 0;
    }

    bool isValidSwitchPoint() override {
        return tag_ && (tag_->type == 0x08 || tag_->type == 0x09) && !tag_->isSeqHeader;
    }
  protected:
    int getTagType() override {
        return tag_ ? tag_->type : 0;
    }
};

class RawFlvTagDataChunk: public FlvTagDataChunk {
  private:
    uint8_t* tag_ = nullptr;
    int tag_size_ = 0;
  public:
    static constexpr int HEADER_LENGTH = 11;

    RawFlvTagDataChunk(RawFlvTagDataChunk&) = delete;
    RawFlvTagDataChunk& operator = (RawFlvTagDataChunk&) = delete;

    RawFlvTagDataChunk(uint8_t* tag, int tag_size): tag_(tag), tag_size_(tag_size) {}
    ~RawFlvTagDataChunk() {
        if (tag_)
            delete[] tag_;
    }

    uint8_t* getData() override { return tag_; }
    uint32_t getSize() override { return tag_size_; }

    int64_t getTimestamp() override {
        return
            ((uint32_t)(tag_[7]) << 24) |
            ((uint32_t)(tag_[4] << 16)) |
            ((uint32_t)(tag_[5] << 8)) |
            ((uint32_t)(tag_[6]));
    }
    bool isValidSwitchPoint() override {
        uint8_t* body = tag_ + HEADER_LENGTH;
        int body_size = tag_size_ - 4 - HEADER_LENGTH;
        if (body_size < 4)
            return false;

        switch (this->getTagType()) {
            case 8: // audio
                return !(10 == ((body[0] & 0xf0) >> 4) && 0 == body[1]); // aac seq header
            case 9: // video
                if (VideoCodecID::AVCVIDEOPACKET == (body[0] & 0x0f))
                    return !(VideoCodecFrameType::KEY_FRAME == (body[0] >> 4) && VideoCodecAVCType::AVC_SEQUENCE_HEADER == body[1]);
                else if (VideoCodecID::HEVCVIDEOPACKET == (body[0] & 0x0f))
                    return !(VideoCodecFrameType::KEY_FRAME == (body[0] >> 4) && VideoCodecAVCType::AVC_SEQUENCE_HEADER == body[1]);
                else
                    return false;
            default:
                return false;
        }
    }

  protected:
    int getTagType() override {
        return tag_[0] & 0x1f;
    }
};

class FlvHeaderDataChunk: public LiveXyP2spHttpDataSource::DataChunk {
  public:
    static constexpr int LENGTH = 13;
  private:
    uint8_t data_[LENGTH];
  public:
    FlvHeaderDataChunk(uint8_t const* data) {
        memcpy(data_, data, LENGTH);
    }
    uint8_t* getData() override { return data_; }
    uint32_t getSize() override { return LENGTH; }
    tag_hash_t getHash() override { return 0; }
    int64_t getTimestamp() override { return 0; }
    bool isValidSwitchPoint() override { return false; }
};

}

#define LOG_PREFIX "[LiveXyP2spHttpDataSource] "

// 该Datasource包含两个数据源：cdn和p2sp，cdn使用default http data source，p2sp使用xy提供的p2sp sdk
// 我们需要在合适的时候选择合适的数据源输出，并且只在需要的时候读取对应的数据源，同时保证数据连续。
//
// 该Datasource对应三个线程：
// - CDN读取线程：维护default http data source的整个生命周期，从它读取数据，解析flv
// - P2SP读取线程：维护p2sp task的整个生命周期，从它读取数据，解析flv
// - 主线程：被调用Open/Read/Close的线程，会从输出缓存中选择相应的数据输出，并且设置标志位控制上述两个线程
//
// 两个读取线程分别将数据写到cdn_tags_buffer_和p2sp_tags_buffer，主线程选择相应的数据输出。
// 主线程会设置prefer_p2sp和using_p2sp两个标志位，分别表示“想要使用p2sp”和“正在使用p2sp”，
// 初始状态为“想要使用cdn”和“正在使用cdn”，即从cdn开播。根据播放器缓存大小的变换，“想要使用”会变化，相应数据源开始读取数据，
// 当想要使用的数据源数据满足时，“正在使用”随之变化（切换成功）。
//
// 解析flv是为了以flv tag为单位找到数据的连接点，主线程输出后会维护一个历史flv tag的哈希值。
// 当想要切换的数据源输出的flv tag在历史中能找到并且距离不远的时候就可以切换。

#define MAX_TAGS_HISTORY_SIZE 1024
#define MAX_TAGS_BUFFER_SIZE 512

#define SWITCH_TIMEOUT_AFTER_FIRST_DATA 5000 // 如果新数据源有数据了5秒但还没切换成功，就认为失败，防止一直浪费流量


LiveXyP2spHttpDataSource::LiveXyP2spHttpDataSource(std::shared_ptr<DownloadManager> download_manager,
                                                   std::shared_ptr<TransferListener<HttpDataSource>> listener,
                                                   DownloadOpts const& opts,
                                                   AwesomeCacheRuntimeInfo* ac_rt_info):
    download_manager_(download_manager),
    listener_(listener),
    download_opts_(opts),
    ac_rt_info_(ac_rt_info) {
    LOG_INFO(LOG_PREFIX "Initializing");
    this->ac_rt_info_->p2sp_task.enabled = 1;
    if (this->download_opts_.player_statistic) {
        this->player_statistic_it_ = this->download_opts_.player_statistic->add_listener(
                                         std::bind(&LiveXyP2spHttpDataSource::UpdatePlayerStatistic, this, std::placeholders::_1));
    }
}

LiveXyP2spHttpDataSource::~LiveXyP2spHttpDataSource() {
    if (this->download_opts_.player_statistic) {
        this->download_opts_.player_statistic->remove_listener(this->player_statistic_it_);
    }
}

int64_t LiveXyP2spHttpDataSource::Read(uint8_t* buf, int64_t offset, int64_t len) {
    if (this->last_read_error_ < 0) {
        LOG_WARN(LOG_PREFIX "Return last read error: %lld", this->last_read_error_);
        return this->last_read_error_;
    }

    if (this->current_reading_ && this->current_reading_offset_ == this->current_reading_->getSize()) {
        this->current_reading_.reset();
        this->current_reading_offset_ = 0;
    }

    while (!this->current_reading_) {
        int64_t ret = this->FindNextTag();
        if (ret < 0) {
            LOG_WARN(LOG_PREFIX "FindNextTag returned error %lld", ret);
            this->last_read_error_ = ret;
            return ret;
        }
    }

    len = std::min(len, (int64_t)this->current_reading_->getSize() - this->current_reading_offset_);
    memcpy(buf + offset, this->current_reading_->getData() + this->current_reading_offset_, len);
    this->current_reading_offset_ += len;

    return len;
}

int64_t LiveXyP2spHttpDataSource::FindNextTag() {
    assert(this->current_reading_ == nullptr);

    LOG_DEBUG(LOG_PREFIX "FindNextTag: prefer_p2sp: %d, using_p2sp: %d, "
              "cdn_switch_out_count: %d, p2sp_switch_out_count: %d, "
              "cdn_tags_buffer_size: %lu, p2sp_tags_buffer_size: %lu, "
              "buffer_len: %d",
              this->prefer_p2sp_, this->using_p2sp_,
              this->cdn_switch_out_count_, this->p2sp_switch_out_count_,
              this->cdn_tags_buffer_.size(), this->p2sp_tags_buffer_.size(),
              this->player_buffer_len_);

    int64_t now = kpbase::SystemUtil::GetCPUTime();
    if (!this->prefer_p2sp_ && !this->using_p2sp_) {
        // using cdn source and prefer cdn source now
        // let's check if we'd like to switch to p2sp source
        if (this->p2sp_switch_out_count_ < this->download_opts_.live_p2sp_switch_max_count &&
            this->player_buffer_len_ > this->download_opts_.live_p2sp_switch_on_buffer_threshold_ms &&
            now - this->player_buffer_len_last_cross_threshold_timestamp_ > this->download_opts_.live_p2sp_switch_on_buffer_hold_threshold_ms &&
            // check CD against last successfully switched timestamp
            now - this->switch_performed_timestamp_ > this->download_opts_.live_p2sp_switch_cooldown_ms &&
            // if cdn read thread already returned error (maybe eof), do not try to switch
            this->cdn_read_error_code_ == 0) {

            LOG_INFO(LOG_PREFIX "Setting prefer_p2sp to true");
            this->prefer_p2sp_ = true;
            this->switch_start_timestamp_ = now;
            this->switch_first_data_timestamp_ = 0;
            this->ctrl_cond_.notify_all();

            this->ac_rt_info_->p2sp_task.p2sp_switch_attempts += 1;
        }
    } else if (this->prefer_p2sp_ && this->using_p2sp_) {
        // using p2sp source and prefer p2sp source now
        // let's check if we'd like to switch back to cdn source
        //
        // always trust cdn, do not check cdn_switch_out_count
        if (this->player_buffer_len_ < this->download_opts_.live_p2sp_switch_off_buffer_threshold_ms &&
            // if p2sp read thread already returned error (maybe eof), do not try to switch
            this->p2sp_read_error_code_ == 0) {

            LOG_INFO(LOG_PREFIX "Setting prefer_p2sp to false");
            this->prefer_p2sp_ = false;
            this->switch_start_timestamp_ = now;
            this->switch_first_data_timestamp_ = 0;
            this->ctrl_cond_.notify_all();

            this->ac_rt_info_->p2sp_task.cdn_switch_attempts += 1;
        }
    } else {
        // prefered source != using source, we're already trying to switch
        // let's check if we should switch now

        std::unique_lock<std::mutex> lock(this->mutex_);
        std::list<std::unique_ptr<DataChunk>>& prefer_tags_buffer =
                                               this->prefer_p2sp_ ? p2sp_tags_buffer_ : cdn_tags_buffer_;
        if (!prefer_tags_buffer.empty() && this->switch_first_data_timestamp_ == 0)
            this->switch_first_data_timestamp_ = now;

        int switch_duration = now - this->switch_start_timestamp_;
        int switch_duration_from_first_data =
            this->switch_first_data_timestamp_ == 0 ? -1 : now - this->switch_first_data_timestamp_;

        // may also check switch_duration here
        if (switch_duration_from_first_data > SWITCH_TIMEOUT_AFTER_FIRST_DATA) {
            LOG_WARN(LOG_PREFIX "Switch to %s did not happen for %d/%d ms, set prefer_p2sp to %d",
                     this->prefer_p2sp_ ? "p2sp" : "cdn",
                     switch_duration, switch_duration_from_first_data,
                     this->using_p2sp_);
            if (this->prefer_p2sp_)
                this->p2sp_switch_out_count_ += 1;
            else
                this->cdn_switch_out_count_ += 1;
            this->prefer_p2sp_ = this->using_p2sp_;
            this->ctrl_cond_.notify_all();
        } else if (!this->tags_history_.empty()) {
            int64_t current_tag_ts = this->tags_history_ts_[this->tags_history_.back()];
            int64_t max_matched_tag_ts = -1;

            // ignore non-valid switch points like metadata
            while (!prefer_tags_buffer.empty() && !prefer_tags_buffer.front()->isValidSwitchPoint())
                prefer_tags_buffer.pop_front();

            while (!prefer_tags_buffer.empty()) {
                auto it = this->tags_history_ts_.find(prefer_tags_buffer.front()->getHash());
                if (it == this->tags_history_ts_.end())
                    break;
                max_matched_tag_ts = it->second;
                prefer_tags_buffer.pop_front();
            }

            LOG_DEBUG(LOG_PREFIX "current_tag_ts: %lld, max_matched_tag_ts: %lld",
                      current_tag_ts, max_matched_tag_ts);
            if (max_matched_tag_ts != -1 && current_tag_ts - max_matched_tag_ts < this->download_opts_.live_p2sp_switch_lag_threshold_ms) {
                this->using_p2sp_ = this->prefer_p2sp_;
                this->ctrl_cond_.notify_all();

                LOG_INFO(LOG_PREFIX "SWITCHED! using_p2sp: %d, current buffer: %lu, switch duration: %d/%d",
                         this->using_p2sp_,
                         this->using_p2sp_ ? this->p2sp_tags_buffer_.size() : this->cdn_tags_buffer_.size(),
                         switch_duration, switch_duration_from_first_data);
                this->switch_performed_timestamp_ = now;

                if (this->using_p2sp_) {
                    this->cdn_switch_out_count_ += 1;
                    this->ac_rt_info_->p2sp_task.p2sp_switch_success_attempts += 1;
                    this->ac_rt_info_->p2sp_task.p2sp_switch_duration_ms += switch_duration;
                } else {
                    this->p2sp_switch_out_count_ += 1;
                    this->ac_rt_info_->p2sp_task.cdn_switch_success_attempts += 1;
                    this->ac_rt_info_->p2sp_task.cdn_switch_duration_ms += switch_duration;
                }
            }
        }

        if (this->prefer_p2sp_ == this->using_p2sp_) {
            // if we changed prefer/using state,
            // then clear the old tags buffer, because the old source would soon quit
            // if it restart, the tags in buffer would not be continous
            if (this->using_p2sp_)
                this->cdn_tags_buffer_.clear();
            else
                this->p2sp_tags_buffer_.clear();
        }

    }

    std::list<std::unique_ptr<DataChunk>>& using_tags_buffer =
                                           this->using_p2sp_ ? p2sp_tags_buffer_ : cdn_tags_buffer_;
    std::condition_variable& using_tags_buffer_cond =
        this->using_p2sp_ ? p2sp_tags_buffer_cond_ : cdn_tags_buffer_cond_;
    int64_t& using_read_error_code_ =
        this->using_p2sp_ ? p2sp_read_error_code_ : cdn_read_error_code_;

    while (true) {
        // remove duplicated tags in loop
        std::unique_lock<std::mutex> lock(this->mutex_);

        int wait_timeout = std::max(
                               100, (int)this->player_buffer_len_ - this->download_opts_.live_p2sp_switch_off_buffer_threshold_ms);
        bool wait_ret = using_tags_buffer_cond.wait_for(lock, std::chrono::milliseconds(wait_timeout), [&, this] {
            return (!using_tags_buffer.empty() ||
                    using_read_error_code_ < 0 ||
                    this->ShouldQuit());
        });
        if (!wait_ret) {
            // timeout here so that if p2sp did not produces any data, we would switch to cdn before player buffer runs out (and other ways around)
            LOG_WARN(LOG_PREFIX "FindNextTag: wait for tags buffer timeout, try again later");
            return 0;
        }

        // check buffer before error code and quit flag
        if (using_tags_buffer.size() > 0) {
            std::unique_ptr<DataChunk> tag = std::move(using_tags_buffer.front());
            using_tags_buffer.pop_front();

            if (this->tags_history_ts_.find(tag->getHash()) != this->tags_history_ts_.end()) {
                LOG_INFO(LOG_PREFIX "Duplicated tag, drop");
                continue;
            } else {
                LOG_DEBUG(LOG_PREFIX "Return tag: size=%u, timestamp=%lld", tag->getSize(), tag->getTimestamp());
                this->current_reading_ = std::move(tag);
                break;
            }
        }

        if (this->ShouldQuit()) {
            LOG_WARN(LOG_PREFIX "FindNextTag: %s thread quitted, return error",
                     this->using_p2sp_ ? "p2sp" : "cdn");
            return kResultEndOfInput;
        }

        if (using_read_error_code_ < 0) {
            LOG_WARN(LOG_PREFIX "FindNextTag: %s thread returned error code: %lld, return",
                     this->using_p2sp_ ? "p2sp" : "cdn", using_read_error_code_);
            return using_read_error_code_;
        }
    }

    if (this->using_p2sp_)
        this->ac_rt_info_->p2sp_task.p2sp_used_bytes += this->current_reading_->getSize();
    else
        this->ac_rt_info_->p2sp_task.cdn_used_bytes += this->current_reading_->getSize();

    // insert new tag into history
    if (!this->tags_history_.empty() && this->current_reading_->getTimestamp() < this->tags_history_ts_[this->tags_history_.back()]) {
        LOG_WARN(LOG_PREFIX "Non-monotonic timestamp: %lld, %lld",
                 this->current_reading_->getTimestamp(),
                 this->tags_history_ts_[this->tags_history_.back()]);
    }
    this->tags_history_ts_[this->current_reading_->getHash()] = this->current_reading_->getTimestamp();
    this->tags_history_.push(this->current_reading_->getHash());

    while (this->tags_history_.size() > MAX_TAGS_HISTORY_SIZE) {
        this->tags_history_ts_.erase(this->tags_history_.front());
        this->tags_history_.pop();
    }

    return 0;
}

bool LiveXyP2spHttpDataSource::ShouldQuit() {
    return this->thread_quit_ ||
           AwesomeCacheInterruptCB_is_interrupted(&this->download_opts_.interrupt_cb);
}


int64_t LiveXyP2spHttpDataSource::Open(DataSpec const& spec) {
    LOG_INFO(LOG_PREFIX "Opening %s", spec.uri.c_str());
    this->data_spec_ = spec;

    this->cdn_read_thread_ = std::thread(&LiveXyP2spHttpDataSource::RunCdnReadThread, this);
    this->p2sp_read_thread_ = std::thread(&LiveXyP2spHttpDataSource::RunP2spReadThread, this);

    std::unique_lock<std::mutex> lock(this->mutex_);
    this->cdn_initial_open_result_cond_.wait(lock, [this] {
        return this->cdn_initial_open_result_valid_;
    });

    LOG_INFO(LOG_PREFIX "Open complete, result: %lld, host: %s",
             this->cdn_initial_open_result_, this->data_spec_host_.c_str());
    return this->cdn_initial_open_result_;
}

AcResultType LiveXyP2spHttpDataSource::Close() {
    LOG_INFO(LOG_PREFIX "Closing");
    {
        std::unique_lock<std::mutex> lock(this->mutex_);
        this->thread_quit_ = true;
        this->ctrl_cond_.notify_all();
    }

    if (this->p2sp_read_thread_.joinable()) {
        LOG_INFO(LOG_PREFIX "Closing: Waiting p2sp read thread");
        this->p2sp_read_thread_.join();
    }
    if (this->cdn_read_thread_.joinable()) {
        LOG_INFO(LOG_PREFIX "Closing: Waiting cdn read thread");
        this->cdn_read_thread_.join();
    }
    LOG_INFO(LOG_PREFIX "Closed");

    return kResultOK;
}

void LiveXyP2spHttpDataSource::UpdatePlayerStatistic(PlayerStatistic const* statistic) {
    if (!statistic)
        return;

    std::lock_guard<std::mutex> lock(this->player_buffer_mutex_);
    int32_t buffer_len =
        std::max((int64_t)0, std::min(statistic->audio_cache_duration_ms, statistic->video_cache_duration_ms));

    int old_state =
        (this->player_buffer_len_ < this->download_opts_.live_p2sp_switch_off_buffer_threshold_ms) ? 0 :
        (this->player_buffer_len_ > this->download_opts_.live_p2sp_switch_on_buffer_threshold_ms) ? 1 : 2;
    int new_state =
        (buffer_len < this->download_opts_.live_p2sp_switch_off_buffer_threshold_ms) ? 0 :
        (buffer_len > this->download_opts_.live_p2sp_switch_on_buffer_threshold_ms) ? 1 : 2;
    if (old_state != new_state) {
        this->player_buffer_len_last_cross_threshold_timestamp_ = kpbase::SystemUtil::GetCPUTime();
    }

    this->player_buffer_len_ = buffer_len;
    LOG_DEBUG(LOG_PREFIX "Setting player buffer length to %d", this->player_buffer_len_);
}


void LiveXyP2spHttpDataSource::RunP2spReadThread() {
    while (true) {
        LOG_INFO(LOG_PREFIX "P2sp read thread waiting to proceed");
        {
            std::unique_lock<std::mutex> lock(this->mutex_);
            this->ctrl_cond_.wait(lock, [this] {
                return this->ShouldQuit() || this->prefer_p2sp_;
            });
        }

        if (this->ShouldQuit()) {
            LOG_INFO(LOG_PREFIX "Got quit signal, exit p2sp read thread");
            break;
        }

        assert(this->p2sp_tags_buffer_.empty());

        LiveXyP2spSdkGuard p2sp_sdk_guard;
        if (!p2sp_sdk_guard.IsValid())
            break;

        // translate uri, replace ip address with host
        std::string uri = this->data_spec_.uri;
        if (!this->data_spec_host_.empty()) {
            int p_begin = uri.find("://");
            if (p_begin != std::string::npos) {
                int p_end = uri.find("/", p_begin + 3);
                if (p_end != std::string::npos) {
                    uri = uri.substr(0, p_begin + 3) + this->data_spec_host_ + uri.substr(p_end);
                }
            }
        }

        LOG_INFO(LOG_PREFIX "Creating p2sp task for %s", uri.c_str());
        int task_id = xydlsym::xylive_sdk_server_createTask(uri.c_str());
        if (task_id < 0) {
            LOG_ERROR(LOG_PREFIX "Create p2sp task failed: %d", task_id);
            this->p2sp_read_error_code_ = kResultExceptionSourceNotOpened_0;
            break;
        }
        LOG_INFO(LOG_PREFIX "Created p2sp task: %d", task_id);

        while (!this->ShouldQuit() &&
               (this->prefer_p2sp_ || this->using_p2sp_)) {
            std::list<xy_flv_tag*> tags_buffer;
            int error_code = 0;

            xydlsym::xylive_sdk_server_getData(task_id, tags_buffer, this->player_buffer_len_, error_code);
            size_t tags_buffer_size = tags_buffer.size();
            LOG_DEBUG(LOG_PREFIX "P2sp getData returned %lu tags", tags_buffer_size);

            if (error_code < 0) {
                LOG_ERROR(LOG_PREFIX "P2sp getData returned error: %d", error_code);
                while (!tags_buffer.empty()) {
                    xy_flv_tag_delete(tags_buffer.front());
                    tags_buffer.pop_front();
                }
                this->p2sp_read_error_code_ = kResultEndOfInput;
                break;
            } else if (tags_buffer_size > 0) {
                std::lock_guard<std::mutex> lock(this->mutex_);
                // do not insert result if this loop is going to break
                // make sure that if this datasource restarts, the tags_buffer would start empty
                // also see the logic where using_p2sp is set
                while ((this->using_p2sp_ || this->prefer_p2sp_) && !tags_buffer.empty()) {
                    this->ac_rt_info_->p2sp_task.p2sp_download_bytes += tags_buffer.front()->tagSize;
                    this->p2sp_tags_buffer_.emplace_back(new XyFlvTagDataChunk(tags_buffer.front()));
                    tags_buffer.pop_front();
                }

                while (!this->using_p2sp_ && this->p2sp_tags_buffer_.size() > MAX_TAGS_BUFFER_SIZE)
                    this->p2sp_tags_buffer_.pop_front();

                this->p2sp_tags_buffer_cond_.notify_all();
            }

            while (!tags_buffer.empty()) {
                xy_flv_tag_delete(tags_buffer.front());
                tags_buffer.pop_front();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        LOG_INFO(LOG_PREFIX "P2sp getData loop breaked, task_id = %d", task_id);
        if (task_id >= 0) {
            LOG_INFO(LOG_PREFIX "Destroying p2sp task %d", task_id);
            xydlsym::xylive_sdk_server_releaseTask(task_id);
        }

        if (this->p2sp_read_error_code_ < 0) {
            LOG_ERROR(LOG_PREFIX "P2sp read error code: %lld, exit read thread", this->p2sp_read_error_code_);
            break;
        }
        // continue read thread
    }

    // notify threads waiting for read
    this->p2sp_tags_buffer_cond_.notify_all();
}

void LiveXyP2spHttpDataSource::RunCdnReadThread() {
    bool header_sent = false;
    while (true) {
        LOG_INFO(LOG_PREFIX "Cdn read thread waiting to proceed");
        {
            std::unique_lock<std::mutex> lock(this->mutex_);
            this->ctrl_cond_.wait(lock, [this] {
                return this->ShouldQuit() || !this->prefer_p2sp_;
            });
        }

        if (this->ShouldQuit()) {
            LOG_INFO(LOG_PREFIX "Got quit signal, exit cdn read thread");
            break;
        }

        assert(this->cdn_tags_buffer_.empty());

        LOG_INFO(LOG_PREFIX "Opening cdn datasource");
        FFUrlHttpDataSource source(this->download_opts_, this->ac_rt_info_);

        int64_t ret = 0;
        ret = source.Open(this->data_spec_);
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->cdn_initial_open_result_ = ret;
            this->cdn_initial_open_result_valid_ = true;
            this->cdn_initial_open_result_cond_.notify_one();
        }

        if (ret < 0) {
            LOG_ERROR(LOG_PREFIX "Open cdn datasource failed: %lld", ret);
            this->cdn_read_error_code_ = ret;
            break;
        }
        LOG_INFO(LOG_PREFIX "Opened cdn datasource, reading header");

        if (this->data_spec_host_.empty())
            this->data_spec_host_ = source.GetConnectionInfo().host;

        uint8_t header_data[FlvHeaderDataChunk::LENGTH];
        int header_size = 0;
        while (header_size < FlvHeaderDataChunk::LENGTH) {
            int64_t len = source.Read(header_data, header_size, FlvHeaderDataChunk::LENGTH - header_size);
            if (len <= 0) {
                LOG_ERROR(LOG_PREFIX "Cdn read error during flv header: %lld", len);
                this->cdn_read_error_code_ = len;
                goto cdn_fail;
            }
            header_size += len;
        }

        if (!header_sent) {
            header_sent = true;
            LOG_INFO(LOG_PREFIX "Sending flv header data chunk");
            std::lock_guard<std::mutex> lock(this->mutex_);
            this->cdn_tags_buffer_.emplace_back(new FlvHeaderDataChunk(header_data));
            this->cdn_tags_buffer_cond_.notify_all();
        }

        while (!this->ShouldQuit() &&
               (!this->prefer_p2sp_ || !this->using_p2sp_)) {

            // read flv tag
            int read_len = 0;
            uint8_t tag_header[RawFlvTagDataChunk::HEADER_LENGTH];
            while (read_len < RawFlvTagDataChunk::HEADER_LENGTH) {
                int64_t len = source.Read(tag_header, read_len, RawFlvTagDataChunk::HEADER_LENGTH - read_len);
                if (len <= 0) {
                    LOG_ERROR(LOG_PREFIX "Cdn read error while reading flv tag header: %lld", len);
                    this->cdn_read_error_code_ = len;
                    goto cdn_fail;
                }
                read_len += len;
            }

            uint32_t tag_data_size =
                ((uint32_t)(tag_header[1]) << 16) |
                ((uint32_t)(tag_header[2]) << 8) |
                ((uint32_t)(tag_header[3]));
            uint32_t tag_size = RawFlvTagDataChunk::HEADER_LENGTH + tag_data_size + 4;

            uint8_t* tag = new (std::nothrow) uint8_t[RawFlvTagDataChunk::HEADER_LENGTH + tag_data_size + 4];
            if (!tag) {
                LOG_ERROR(LOG_PREFIX "Cannot allocate, tag_data_size=%u", tag_data_size);
                this->cdn_read_error_code_ = kResultFFurlUnknown;
                goto cdn_fail;
            }
            memcpy(tag, tag_header, RawFlvTagDataChunk::HEADER_LENGTH);

            while (read_len < tag_size) {
                int64_t len = source.Read(tag, read_len, tag_size - read_len);
                if (len <= 0) {
                    LOG_ERROR(LOG_PREFIX "Cdn read error while reading flv tag body: %lld", len);
                    this->cdn_read_error_code_ = len;
                    delete[] tag;
                    goto cdn_fail;
                }
                read_len += len;
            }

            std::unique_ptr<DataChunk> data_chunk(new RawFlvTagDataChunk(tag, tag_size));
            LOG_DEBUG(LOG_PREFIX "Got flv tag from cdn: size=%d", tag_size);
            this->ac_rt_info_->p2sp_task.cdn_download_bytes += tag_size;

            std::lock_guard<std::mutex> lock(this->mutex_);

            // do not insert result if this loop is going to break
            // make sure that if this datasource restarts, the tags_buffer would start empty
            // also see the logic where using_p2sp is set
            if (!this->using_p2sp_ || !this->prefer_p2sp_)
                this->cdn_tags_buffer_.push_back(std::move(data_chunk));

            while (this->using_p2sp_ && this->cdn_tags_buffer_.size() > MAX_TAGS_BUFFER_SIZE)
                this->cdn_tags_buffer_.pop_front();

            this->cdn_tags_buffer_cond_.notify_all();
        }

cdn_fail:
        LOG_INFO(LOG_PREFIX "Cdn read loop breaked, closing datasource");
        source.Close();

        if (this->cdn_read_error_code_ < 0) {
            LOG_ERROR(LOG_PREFIX "Cdn read error code: %lld, exit read thread", this->cdn_read_error_code_);
            break;
        }

        // continue read thread
    }

    // notify threads waiting for read
    this->cdn_tags_buffer_cond_.notify_all();
}

#endif
