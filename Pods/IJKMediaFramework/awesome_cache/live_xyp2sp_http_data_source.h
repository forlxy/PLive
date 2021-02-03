#ifdef CONFIG_LIVE_P2SP

#ifndef AWESOME_CACHE_LIVE_XYP2SP_HTTP_DATA_SOURCE_H
#define AWESOME_CACHE_LIVE_XYP2SP_HTTP_DATA_SOURCE_H

#include <thread>
#include <queue>
#include <list>
#include <string>
#include <unordered_map>
#include <condition_variable>

#include "default_http_data_source.h"

#include "xylivesdk/xy_format.hpp"

namespace kuaishou {
namespace cache {

class LiveXyP2spHttpDataSource final : public HttpDataSource {
  public:
    // wrapper over xy_flv_tag
    class DataChunk {
      public:
        using tag_hash_t = uint64_t;

        virtual ~DataChunk() {};
        virtual uint8_t* getData() = 0;
        virtual uint32_t getSize() = 0;
        virtual int64_t getTimestamp() = 0;
        virtual tag_hash_t getHash() = 0;
        virtual bool isValidSwitchPoint() = 0;
    };

  public:
    LiveXyP2spHttpDataSource(std::shared_ptr<DownloadManager> download_manager,
                             std::shared_ptr<TransferListener<HttpDataSource>> listener,
                             DownloadOpts const& opts,
                             AwesomeCacheRuntimeInfo* ac_rt_info);
    ~LiveXyP2spHttpDataSource();

    virtual int64_t Open(DataSpec const& spec) override;
    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t len) override;
    virtual AcResultType Close() override;
    virtual Stats* GetStats() override { return &this->stats_; }
    void UpdatePlayerStatistic(PlayerStatistic const* statistic);

    void RunCdnReadThread();
    void RunP2spReadThread();

  private:
    int64_t FindNextTag();
    bool ShouldQuit();

  private:
    std::shared_ptr<DownloadManager> download_manager_;
    std::shared_ptr<TransferListener<HttpDataSource>> listener_;
    DownloadOpts download_opts_;
    DataSpec data_spec_;
    std::string data_spec_host_; // url host from http data source, useful for p2sp task
    AwesomeCacheRuntimeInfo* ac_rt_info_;
    PlayerStatistic::listeners_iterator_t player_statistic_it_;

  private:
    // currently unused
    HttpDataSourceStats stats_;

    std::mutex player_buffer_mutex_;
    uint32_t player_buffer_len_ = 0;
    // when is the last time did player_buffer_len cross switch_on/off_buffer_threshold
    // this is used to test how long has player_buffer_len been above/below threshold
    int64_t player_buffer_len_last_cross_threshold_timestamp_ = 0;

    // it seems that Read() would be called several more times even after an error is returned
    // I record it here so that cdn/p2sp threads would not be affected after error is returned
    int64_t last_read_error_ = 0;

    std::thread cdn_read_thread_, p2sp_read_thread_;
    int64_t cdn_read_error_code_ = 0, p2sp_read_error_code_ = 0;
    bool thread_quit_ = false;

    std::mutex mutex_;

    // result of Open() of cdn datasource, used by Open()
    int64_t cdn_initial_open_result_;
    bool cdn_initial_open_result_valid_ = false;
    std::condition_variable cdn_initial_open_result_cond_;

    // p2sp/cdn switch control
    std::condition_variable ctrl_cond_;
    // prefer p2sp/cdn means that we would like to use p2sp/cdn and is trying to do that
    bool prefer_p2sp_ = false;
    // using p2sp/cdn means that we are reading data from p2sp/cdn now
    bool using_p2sp_ = false;
    // when did we switch prefered source
    uint64_t switch_start_timestamp_ = 0;
    // when did prefered source get first data
    uint64_t switch_first_data_timestamp_ = 0;
    // when did we successfully switched
    uint64_t switch_performed_timestamp_ = 0;
    // Failed attempts to switch to cdn/p2sp source
    int cdn_switch_out_count_ = 0, p2sp_switch_out_count_ = 0;

    // saved history of hash of tags that are already been read
    std::queue<DataChunk::tag_hash_t> tags_history_;
    std::unordered_map<DataChunk::tag_hash_t, int64_t> tags_history_ts_;

    // std::list<xy_flv_tag*> p2sp_tags_buffer_, cdn_tags_buffer_;
    std::list<std::unique_ptr<DataChunk>> p2sp_tags_buffer_, cdn_tags_buffer_;
    std::condition_variable p2sp_tags_buffer_cond_, cdn_tags_buffer_cond_;

    std::unique_ptr<DataChunk> current_reading_;
    int current_reading_offset_ = 0;
};

}
}



#endif

#endif
