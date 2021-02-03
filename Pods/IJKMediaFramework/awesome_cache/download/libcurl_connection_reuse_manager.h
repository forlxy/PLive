#pragma once

extern "C" {
#include <curl/curl.h>
}

#include "cache_opts.h"

#include <string>

namespace kuaishou {
namespace cache {

// Manage curl shared connections
// So that difference instances of curl for different videos can maybe re-use existing tcp connections
//
// All static methods are thread-safe
class LibcurlConnectionReuseManager {
  public:
    // Setup tcp connection reuse related params for curl, if enabled
    static void Setup(CURL* curl, DownloadOpts const& opts);
    // Must call `teardown` for every pointer that has been called with `setup`
    static void Teardown(CURL* curl);
    // Call this function on network change, with new network id
    static void OnNetworkChange(std::string network_id);
};

}
}
