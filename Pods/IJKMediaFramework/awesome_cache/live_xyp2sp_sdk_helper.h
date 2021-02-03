#ifdef CONFIG_LIVE_P2SP

#ifndef AWESOME_CACHE_LIVE_XYP2SP_SDK_HELPER_H
#define AWESOME_CACHE_LIVE_XYP2SP_SDK_HELPER_H

#include <cstdint>
#include <string>
#include <list>

#include "xylivesdk/xy_format.hpp"

namespace kuaishou {
namespace cache {

class LiveXyP2spSdkGuard {
  public:
    // Set the linger duration for p2sp sdk.
    // Aka, how long after the last use of the sdk should it be released
    static void SetSdkLingerDurationMs(int ms);

    // RAII style p2sp sdk holder, so that the sdk would not be freed during holding period
    LiveXyP2spSdkGuard();
    ~LiveXyP2spSdkGuard();

    bool IsValid();
};


// Wrapped xy functions for dynamic loading
namespace xydlsym {
extern void (*xy_flv_tag_destruct)(xy_flv_tag*);
extern int (*xylive_sdk_server_init)(bool waitFinish);
extern int (*xylive_sdk_server_release)(bool waitFinish);
extern int (*xylive_sdk_server_createTask)(const char* url);
extern int (*xylive_sdk_server_getData)(int taskid, std::list<xy_flv_tag*>& tags, uint32_t player_buffer_len, int& errcode);
extern int (*xylive_sdk_server_releaseTask)(int taskid);
}

extern void xy_flv_tag_delete(xy_flv_tag*);

}
}

#endif /* ifndef AWESOME_CACHE_LIVE_XYP2SP_SDK_HELPER_H */

#endif
