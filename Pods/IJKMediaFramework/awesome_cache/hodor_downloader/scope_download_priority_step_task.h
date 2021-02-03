//
// Created by MarshallShuai on 2019-08-05.
//
#pragma once

#include <set>
#include "download_priority_step_task.h"
#include "utils/macro_util.h"
#include "v2/net/scope_curl_http_task.h"
#include "cache_opts.h"
#include "v2/cache/cache_content_v2_with_scope.h"
#include "v2/cache/cache_scope.h"
#include "v2/data_source/async_scope_data_source.h"
#include "awesome_cache_callback.h"


HODOR_NAMESPACE_START

/**
 * 这个类主要用来处理分片储存的下载任务
 */
class ScopeDownloadPriorityStepTask : public DownloadPriorityStepTask {
  public:
    ScopeDownloadPriorityStepTask(const DataSpec& spec, const DownloadOpts& opts,
                                  std::shared_ptr<AwesomeCacheCallback> callback = nullptr,
                                  int main_priority = Priority_HIGH, int sub_priority = 0,
                                  EvictStrategy evict_strategy = EvictStrategy_LRU);

    virtual AcResultType StepExecute(int thread_work_id) override;

    virtual void Interrupt() override ;

    virtual float GetProgressPercent() override;

    virtual AcResultType LastError() override;
  private:
    /**
     * 下载一个scope，如果下载完后，总任务完成了，则会标记complete_=true
     * @param scope 如果为null，表示第一个scope, 这种情况下会负责更新CacheContentIndex里的CacheContent内容
     * @return 错误码
     */
    AcResultType DownloadScope(std::shared_ptr<CacheScope> scope, int thread_work_id);

  private:
    AcResultType last_error_;

    std::shared_ptr<CacheContentV2WithScope> cache_content_;
    std::vector<std::shared_ptr<CacheScope>> scope_list_;
    /**
     * 这个set是缓存scope是否完全缓存的信息
     * 大前提是为了减少访问文件系统的次数，大的前提假设是，scope的文件在StepTask执行过程中不会轻易被破坏
     * 存入的key是scope的start_position
     */
    std::set<int64_t> fully_cached_scope_set_;

    DataSpec spec_;
    DownloadOpts download_opts_;

    /**
     * 用ScopeDataSource来完成download scope的功能
     */
    std::shared_ptr<AsyncScopeDataSource> scope_downloader_;
    bool is_abort_by_interrupt_;

    // 预加载任务是最上层的对象，应该管理callback生命周期
    std::shared_ptr<AwesomeCacheCallback> callback_;
    EvictStrategy evict_strategy_;
};

HODOR_NAMESPACE_END
