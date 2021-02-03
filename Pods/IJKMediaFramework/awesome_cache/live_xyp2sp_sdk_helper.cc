#ifdef CONFIG_LIVE_P2SP

#include <mutex>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <dlfcn.h>
#include "ac_log.h"

#include "./live_xyp2sp_sdk_helper.h"


namespace {
std::chrono::milliseconds s_linger_duration(30 * 60 * 1000);  // default to 30 minutes

std::mutex s_mutex;
std::condition_variable s_cond;
std::thread s_release_thread;
bool s_so_loaded = false;
bool s_inited = false;
int s_ref_count = 0;
std::chrono::time_point<std::chrono::steady_clock> s_release_at;
}

using namespace kuaishou::cache;

void (*xydlsym::xy_flv_tag_destruct)(xy_flv_tag*);
int (*xydlsym::xylive_sdk_server_init)(bool waitFinish);
int (*xydlsym::xylive_sdk_server_release)(bool waitFinish);
int (*xydlsym::xylive_sdk_server_createTask)(const char* url);
int (*xydlsym::xylive_sdk_server_getData)(int taskid, std::list<xy_flv_tag*>& tags, uint32_t player_buffer_len, int& errcode);
int (*xydlsym::xylive_sdk_server_releaseTask)(int taskid);

// return true on success
static bool dlopen_xysdk() {
    void* handle = dlopen("libxylivesdk.so", RTLD_LAZY);
    if (!handle)
        return false;

#define LOAD_SYM(FUNC, SYM) do { \
        xydlsym::FUNC = (decltype(xydlsym::FUNC))dlsym(handle, SYM); \
        if (!xydlsym::FUNC) return false; \
    } while(0)

    LOAD_SYM(xy_flv_tag_destruct, "_ZN10xy_flv_tagD1Ev");
    LOAD_SYM(xylive_sdk_server_init, "_ZN17xylive_sdk_server4initEb");
    LOAD_SYM(xylive_sdk_server_release, "_ZN17xylive_sdk_server7releaseEb");
    LOAD_SYM(xylive_sdk_server_createTask, "_ZN17xylive_sdk_server10createTaskEPKc");
    LOAD_SYM(xylive_sdk_server_getData, "_ZN17xylive_sdk_server7getDataEiRNSt6__ndk14listIP10xy_flv_tagNS0_9allocatorIS3_EEEEjRi");
    LOAD_SYM(xylive_sdk_server_releaseTask, "_ZN17xylive_sdk_server11releaseTaskEi");

#undef LOAD_SYM

    return true;
}

void kuaishou::cache::xy_flv_tag_delete(xy_flv_tag* tag) {
    if (!tag)
        return;
    xydlsym::xy_flv_tag_destruct(tag);
    free(tag);
}

static void release_thread_run() {
    while (true) {
        std::unique_lock<std::mutex> lock(s_mutex);

        if (s_inited && s_ref_count == 0)
            s_cond.wait_until(lock, s_release_at);
        else
            s_cond.wait(lock);

        if (s_inited && s_ref_count == 0 && std::chrono::steady_clock::now() >= s_release_at) {
            // release!
            LOG_INFO("LiveXyP2spSDKHelper: Releasing");
            xydlsym::xylive_sdk_server_release(true);
            s_inited = false;
        }
    }
}

void LiveXyP2spSdkGuard::SetSdkLingerDurationMs(int ms) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (ms >= 0) {
        s_linger_duration = std::chrono::milliseconds(ms);
    }
}

LiveXyP2spSdkGuard::LiveXyP2spSdkGuard() {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_ref_count += 1;

    if (!s_so_loaded) {
        LOG_INFO("LiveXyP2spSDKHelper: Loading libxylivesdk.so");
        s_so_loaded = dlopen_xysdk();
    }
    if (!s_so_loaded) {
        LOG_ERROR("LiveXyP2spSDKHelper: Load libxylivesdk.so failed!");
        return;
    }

    if (!s_inited) {
        LOG_INFO("LiveXyP2spSDKHelper: Initializing");
        xydlsym::xylive_sdk_server_init(true);
        s_inited = true;
    }

// NOTE:
// 当前，Proxy P2SP（由APP控制）和Native P2SP（我）使用了同一个库，包括相同的init/release。
// 因此为了防止出现问题，暂时P2SP库的init/release由APP控制，这里不会定时释放。
// 根据APP的逻辑，若打开任意P2SP，直播过程中肯定已经被初始化并且不会被释放。
// 使用APP的时候，上面的初始化逻辑也是不需要的，但是为了在kwaiplayer demo中也能使用，保留上面的初始化逻辑（多次初始化无副作用）。
// 有朝一日废弃Proxy P2SP的时候，需要把以下两行打开。
#if 0
    if (!s_release_thread.joinable()) {
        s_release_thread = std::thread(release_thread_run);
    }
#endif
}

LiveXyP2spSdkGuard::~LiveXyP2spSdkGuard() {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_ref_count -= 1;
    if (s_ref_count == 0) {
        s_release_at = std::chrono::steady_clock::now() + s_linger_duration;
        s_cond.notify_all();
    }
}

bool LiveXyP2spSdkGuard::IsValid() {
    return s_so_loaded && s_inited;
}

#endif
