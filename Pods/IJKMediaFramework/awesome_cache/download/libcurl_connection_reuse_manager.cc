#include "./libcurl_connection_reuse_manager.h"
#include "ac_log.h"

#include <mutex>
#include <memory>
#include <unordered_map>

using namespace kuaishou::cache;

#define LOG_PREFIX "[LibcurlConnectionReuseManager] "

namespace {

const static bool kVerbose = false;

// mtx is for LibcurlConnectionReuseManager
// curl_share_mtx is for CURLSH
std::mutex mtx, curl_share_mtx;

// If network id is changed, we should use a new CURLSH and drop the old ones
// However, the old ones may be stilled used by other CURL handles,
// so we need to track them using shared pointers.
std::string current_network_id;
std::shared_ptr<CURLSH> current_curl_share;
std::unordered_map<CURL*, std::shared_ptr<CURLSH>> ref_instances;

void curlsh_global_lock(CURL*, curl_lock_data, curl_lock_access, void*) {
    curl_share_mtx.lock();
}

void curlsh_global_unlock(CURL*, curl_lock_data, void*) {
    curl_share_mtx.unlock();
}

//int debug_callback(CURL* handle, curl_infotype typ, char* data, size_t size, void* userdata) {
//    char* s = new char [size + 1];
//    memcpy(s, data, size);
//    s[size] = '\0';
//    if (typ == CURLINFO_TEXT)
//        LOG_INFO(LOG_PREFIX "CURL DEBUG: %s", s);
//    delete [] s;
//    return 0;
//}

}

void LibcurlConnectionReuseManager::OnNetworkChange(std::string network_id) {
    LOG_INFO(LOG_PREFIX "OnNetworkChange: %s", network_id.c_str());
    std::lock_guard<std::mutex> lock(mtx);
    if (current_network_id == network_id)
        return;
    // init a new CURLSH
    LOG_INFO(LOG_PREFIX "Initializing new curlsh...");
    CURLSH* s = curl_share_init();
    if (!s)
        return;
    curl_share_setopt(s, CURLSHOPT_LOCKFUNC, curlsh_global_lock);
    curl_share_setopt(s, CURLSHOPT_UNLOCKFUNC, curlsh_global_unlock);
    curl_share_setopt(s, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

    current_network_id = network_id;
    current_curl_share.reset(s, [](CURLSH * p) { if (p) curl_share_cleanup(p); });
    LOG_INFO(LOG_PREFIX "Initializing new curlsh...done");
}


void LibcurlConnectionReuseManager::Setup(CURL* curl, DownloadOpts const& opts) {
    if (opts.tcp_connection_reuse <= 0)
        return;

    if (kVerbose)
        LOG_VERBOSE(LOG_PREFIX "Setup...libcurl: %s. Network id: %s, params: %d/%d/%d/%d",
                    curl_version(), opts.network_id.c_str(),
                    opts.tcp_keepalive_idle, opts.tcp_keepalive_interval,
                    opts.tcp_connection_reuse, opts.tcp_connection_reuse_maxage);

    std::lock_guard<std::mutex> lock(mtx);
    if (!current_curl_share || !curl)
        return;

    ref_instances.emplace(curl, current_curl_share);
    curl_easy_setopt(curl, CURLOPT_SHARE, current_curl_share.get());

    if (opts.tcp_keepalive_idle > 0 && opts.tcp_keepalive_interval > 0) {
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, (long)1);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, (long)opts.tcp_keepalive_idle);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, (long)opts.tcp_keepalive_interval);
    }
    if (opts.tcp_connection_reuse_maxage > 0) {
        curl_easy_setopt(curl, CURLOPT_MAXAGE_CONN, (long)opts.tcp_connection_reuse_maxage);
    }
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, (long)opts.tcp_connection_reuse);

    // curl_easy_setopt(curl, CURLOPT_VERBOSE, (long)1);
    // curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);

    if (kVerbose)
        LOG_VERBOSE(LOG_PREFIX "Setup...done");
}

void LibcurlConnectionReuseManager::Teardown(CURL* curl) {
    if (kVerbose)
        LOG_VERBOSE(LOG_PREFIX "Teardown...");
    std::lock_guard<std::mutex> lock(mtx);
    if (!curl)
        return;

    curl_easy_setopt(curl, CURLOPT_SHARE, NULL);
    ref_instances.erase(curl);
    if (kVerbose)
        LOG_VERBOSE(LOG_PREFIX "Teardown...done");
}
