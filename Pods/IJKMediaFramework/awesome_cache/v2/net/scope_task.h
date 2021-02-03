#pragma once

#include "./connection_info_v2.h"
#include "cache_opts.h"
#include "awesome_cache_runtime_info_c.h"

namespace kuaishou {
namespace cache {

class ScopeTaskListener {
  public:
    virtual ~ScopeTaskListener() {};
    /**
     * 如果解析头失败，或者直接连接失败，就不会回调
     */
    virtual void OnConnectionInfoParsed(const ConnectionInfoV2& info) = 0;
    virtual void OnReceiveData(uint8_t* data_buf, int64_t data_len) = 0;
    /**
     * 如果连接失败，没收到数据，都不会回调
     */
    /**
     * 一定会回调
     */
    virtual void OnDownloadComplete(int32_t error, int32_t stop_reason) = 0;
};


class ScopeTask {
  public:

    virtual ~ScopeTask() {}

    /**
     * 如果出错则返回error，如果成功open，则返回content_length（注意，不等于总长度，只有spec.position=0的时候才等于总长度）
     */
    virtual int64_t Open(const DataSpec& spec) = 0;

    /**
     * 中断任务
     */
    virtual void Abort() = 0;

    /**
      * 立即终止下载进度,并清理资源
      */
    virtual void Close() = 0;

    /**
     * 等待自然结束
     */
    virtual void WaitForTaskFinish() = 0;


    virtual ConnectionInfoV2 const& GetConnectionInfo() = 0;

    /**
     * 多个scope之间是否应该可以多次调用Open
     * 否则需要重新new一个ScopeTask
     */
    virtual bool CanReopen() { return false; }

  protected:
    // 模拟服务器不支持Range请求的case，这块代码永远返回false即可（不可改为true！）
    // 单元测试会集成这个类然后override为true
    virtual bool MockServerRangeNotSupport() {return false;}

  public:
    static std::shared_ptr<ScopeTask> CreateTask(const DownloadOpts& opts,
                                                 ScopeTaskListener* listener,
                                                 AwesomeCacheRuntimeInfo* ac_rt_info);
};

}
}
