//
// Created by MarshallShuai on 2019-07-01.
//

#pragma once

#include <stats/json_stats.h>
#include <utility.h>
#include <include/awesome_cache_callback.h>
#include "v2/cache/cache_scope.h"
#include "data_source_seekable.h"
#include "v2/net/scope_task.h"
#include "cache_opts.h"
#include "include/cache_session_listener.h"
#include <runloop.h>
#include "utils/macro_util.h"
#include "utils/aes_decrypt.h"

namespace kuaishou {
namespace cache {

/**
 * 1.只负责一个scope范围内的数据读取 Open/Read/Seek/Close
 * 2.同时负责数据补洞（本地没有的数据，会按需从网上下载）
 * 3.一次性文件读取，一次性文件flush/以及flush的时机
 * 4.网络回调(CacheCallback)
 */
class AsyncScopeDataSource final : public ScopeTaskListener {
  public:
    AsyncScopeDataSource(const DownloadOpts& opts,
                         std::shared_ptr<CacheSessionListener> listener = nullptr,
                         AwesomeCacheCallback_Opaque ac_callback = nullptr,
                         AwesomeCacheRuntimeInfo* ac_rt_info = nullptr);

    ~AsyncScopeDataSource();

    /**
     * 表示AsyncScopeDataSource当前的用途
     */
    typedef enum Usage {
        PlayerDataSource = 0,
        DownloadTask = 1,
    } Usage;

    void SetUsage(Usage usage) {
        usage_ = usage;
    }

    /**
     * 如果创建的播放任务的话，等播放完毕
     * @return 表示在下载过程中遇到的错误（last_error);
     */
    AcResultType WaitForDownloadFinish();

    /**
     * 中断任务
     */
    void Abort();

    void SetContextId(int id);
    /**
     *
     * @param expect_consume_length 正常来说，scopeDataSource会下载完自己负责的dataSource，如果外部想让scopeDataSource只需要满足前一段数据长度即可，
     *                              则需要设置expect_consume_length这个字段。但是注意，由于CacheV2的分片设计方法scopeDataSource
     *                              暂时不支持从scope的中间开始缓存，这方面暂时也看不到强需求
     * @return open返回值
     */
    int64_t Open(const std::shared_ptr<CacheScope> scope,
                 std::string& url, int64_t expect_consume_length = kLengthUnset);

    int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len);

    /**
     * 可能会block
     * @param pos 返回当前read_ptr在 AsyncScopeDataSource 内部的offset（注意，不是总偏移）
     * @return seek返回值
     */
    int64_t Seek(int64_t pos);

    void Close();

    /**
     * @return scope起始position
     */
    int64_t GetStartPosition();

    /**
     * @return scope结束position
     */
    int64_t GetEndPosition();

    /**
     * pos是否在这个scope的范围
     * @param pos 位置
     * @return pos是否在这个scope的范围
     */
    bool ContainsPosition(int64_t pos);

    /**
     * 这个接口只是为了暂时兼容老的CacheSessionListner用的，后期会删除！
     * 因为现在RunLoop放在ScopeDataSource维护
     * @param func function
     */
    void PostEventInCallbackThread(function<void()> func, bool shouldWait = false);

#pragma mark ScopeTaskListener
    virtual void OnConnectionInfoParsed(const ConnectionInfoV2& info) override;
    virtual void OnReceiveData(uint8_t* data_buf, int64_t data_len) override ;
    virtual void OnDownloadComplete(int32_t error, int32_t stop_reason) override ;

    struct {
        int64_t total_recv_len_ = 0;
        int64_t total_output_len_ = 0;
    } debug_;
    // 主要给unit test用
    int DebugGetLastError() {return last_error_;}

  private:
    /**
     * 明文形式read
     */
    int64_t ReadPlain(uint8_t* buf, int64_t offset, int64_t read_len);

    /**
     * 明文形式seek
     */
    int64_t SeekPlain(int64_t pos);

    /**
     * 密文形式read，所有解密流程都在此函数完成
     */
    int64_t ReadCrypt(uint8_t* buf, int64_t offset, int64_t read_len);

    /**
     * 密文形式seek，seek过程不会解密
     */
    int64_t SeekCrypt(int64_t pos);

    /**
     * 从本地文件恢复到内存
     * 性能：1M文件一次读取大概在1~10ms左右，绝大部分时候分布在3~5ms左右
     * @return false表示遇到异常情况，true表示成功（文件不存在也属于成功的一种case）
     */
    bool TryResumeFromScopeCacheFile();

    /**
     * DataSource重新打开的时候，需要重置buffer相关的数据
     */
    inline void ResetBufferStatus();

    /**
     *
     * @return true表示flush到本地成功或者没有需要flush到本地的必要（无新下载的数据），false表示有文件系统方面异常
     */
    bool FlushToScopeCacheFileIfNeeded();

    /**
     *
     * @param range_start range起始
     * @param expect_length 长度
     * @return 返回值 文件总长度
     */
    int64_t OpenDownloadTask(int64_t range_start, int64_t expect_length);

    /**
     * 带时间阈值过滤的通知进度
     */
    void ThrottleNotifyProgressToCallbacks(int64_t total_progress_bytes);
    /**
     * 通知AwesomeCacheCallback进度，并且有一定时间阈值（比如50ms）
     */
    void NotifyProgressToAcCallback();

    /**
     * 通知老的cache回调进度
     */
    void NotifyProgressToCacheSessionListener(int64_t total_progress_bytes);

    /**
     * 如果本datasource是player的数据源，则下载完成后需要通知hodorDownloader去resume
     */
    void ResumeHodorDownloaderIfNeeded();
  private:
    int context_id_ = -1;
    std::string url_;
    std::shared_ptr<CacheScope> cache_scope_;
    int32_t last_error_;

    uint8_t* scope_buf_;
    int64_t scope_buf_len_;

    DownloadOpts download_opts_;
    std::shared_ptr<ScopeTask> download_task_;
    std::mutex download_task_mutex_;

    std::unique_ptr<kpbase::Event> download_update_event_;
    int64_t current_read_offset_;

    int64_t init_cache_buf_len_;    // 表示open的时候 本scope 有多少数据已经是cache的了
    int64_t valid_cache_buf_len_;   // 表示本data source在运行过程中 本scope 补充下载数据后实际缓存的数据

    int64_t total_cached_bytes_when_open_; // 表示datasource open的时候，已经缓存了多少cache字节，主要用于回调计算progress

    bool content_length_unknown_on_open_; // 表示open的时候不知道content-length，这样在ConnectionInfo得到的时候有必要更新一下expect_consume_length_
    int64_t expect_consume_length_; // 表示本scope data source完成准备好多少字节就完成任务

    // new callback
    AwesomeCacheCallback* ac_callback_;
    std::shared_ptr<AcCallbackInfo> ac_cb_info_;
    std::unique_ptr<kpbase::Runloop> callback_runloop_;
    int64_t last_notify_progress_ts_ms_;
    int progress_cb_interval_ms_;

    // runtime info
    AwesomeCacheRuntimeInfo* ac_rt_info_;

    // old callback
    std::shared_ptr<CacheSessionListener> cache_session_listener_;

    Usage usage_ = PlayerDataSource;
    // 最多只通知一次
    bool notified_hodor_download_to_resume_ = false;

    // encrypt
    std::shared_ptr<AesDecrypt> aes_dec_;
};


} // namespace cache
} // namespace kuaishou
