#ifndef IJKPLAYER_CONSTANT_H
#define IJKPLAYER_CONSTANT_H

#include <stdint.h>
namespace kuaishou {
namespace cache {

static const int32_t kTimeZone = 8;

static const int32_t kTimeoutUnSet = -1;

/**
* Default maximum single cache file size.
*/
static const long kDefaultMaxCacheFileSize = 2 * 1024 * 1024;
static const long kDefaultBufferOutputStreamSize = 3 * 1024 * 1024;


/**
 * Prefix
 */
static const char* kHttpProtocolPrefix = "http://";
static const char* kHttpsProtocolPrefix = "https://";
static const char* kFileProtocolPrefix = "file://";

static const bool kReportStats = true;
#define RETURN_IF_DISABLE \
    if (!kReportStats) { \
        return; \
    }

} // namespace cache
} // namespace kuaishou

#include "cache_defs.h"
#include "cache_errors.h"

#endif //IJKPLAYER_CONSTANT_H
