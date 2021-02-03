#include "scope_task.h"
#include "scope_curl_http_task.h"
#include "scope_cronet_http_task.h"
#include "scope_p2sp_task.h"


namespace kuaishou {
namespace cache  {

std::shared_ptr<ScopeTask> ScopeTask::CreateTask(const DownloadOpts& opts,
                                                 ScopeTaskListener* listener,
                                                 AwesomeCacheRuntimeInfo* ac_rt_info) {
    switch (opts.upstream_type) {
#ifdef CONFIG_AEGON
        case kCronetHttpDataSource:
            if (ScopeCronetHttpTask::IsEnabled()) {
                if (ac_rt_info) {
                    ac_rt_info->cache_applied_config.upstream_type = kCronetHttpDataSource;
                }
                return std::make_shared<ScopeCronetHttpTask>(opts, listener, ac_rt_info);
            } else {
                if (ac_rt_info) {
                    ac_rt_info->cache_applied_config.upstream_type = kDefaultHttpDataSource;
                }
                return std::make_shared<ScopeCurlHttpTask>(opts, listener, ac_rt_info);
            }
#endif
#ifdef CONFIG_VOD_P2SP
        case kP2spHttpDataSource:
            if (ac_rt_info) {
                ac_rt_info->cache_applied_config.upstream_type = kP2spHttpDataSource;
            }
            return std::make_shared<ScopeP2spTask>(opts, listener, ac_rt_info);
#endif
        case kDefaultHttpDataSource:
        default:
            if (ac_rt_info) {
                ac_rt_info->cache_applied_config.upstream_type = kDefaultHttpDataSource;
            }
            return std::make_shared<ScopeCurlHttpTask>(opts, listener, ac_rt_info);
    }
}

}

}
