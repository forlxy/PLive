#pragma once

#include "awesome_cache_interrupt_cb_c.h"
#include "hodor_config.h"

#define PUBLIC_FOR_UNIT_TEST public

typedef enum {
    kDataSourceTypeUnknown = -1,
    /**
     * The default cache type, the cache will be written following the reading actions.
     */
    kDataSourceTypeDefault = 0,
    /**
     * The Async download mode, the cache will be written following the download thread.
     */
    kDataSourceTypeAsyncDownload,
    /**
     * Datasource type for normal live stream (e.g. flv url), which does not cache data nor allows seeking
     */
    kDataSourceTypeLiveNormal,
    /**
     * Datasource type for adaptive live stream (manifest), not implemented yet
     */
    kDataSourceTypeLiveAdaptive,
    /**
     * Datasource type for segment, such as hls
     */
    kDataSourceTypeSegment,
    /**
     * Datasource type for async v2
     */
    kDataSourceTypeAsyncV2,

} DataSourceType;

typedef enum {
    /**
     * The default curl type.
     */
    kCurlTypeDefault = 0,
    /**
     * The Async download curl.
     */
    kCurlTypeAsyncDownload,
} CurlType;

typedef enum {
    kEvictorNoOp = 0,
    kEvictorLru = 1,
} CacheEvictorStrategy;

typedef enum {
    kDownloadNoOp = 0,
    kDownloadSimplePriority,
} DownloadStratergy;

typedef enum {
    kCacheDir = 0,
    kMaxCacheBytes,
    kMaxCacheFileSize,
    kCacheEvictorStratergy,
    kDownloadManagerStratergy,
    kEnableDetailedStats,
    kHttpProxyAddress,
} CacheOpt;

typedef enum {
    kPriorityHigh,
    kPriorityDefault,
    kPriorityLow,
} DownloadPriority;

typedef enum {
    kDefaultHttpDataSource = 0,
    kMultiDownloadHttpDataSource,
    kFFUrlHttpDataSource,
    kP2spHttpDataSource,
    kCronetHttpDataSource,
} UpstreamDataSourceType;

typedef enum {
    kBufferedDataSource = 0,
} BufferedDataSourceType;

typedef enum {
    kDownloadStopReasonUnset = -1,  // only for qos report
    kDownloadStopReasonUnknown = 0,
    kDownloadStopReasonFinished = 1,
    kDownloadStopReasonCancelled = 2,
    kDownloadStopReasonFailed = 3,
    kDownloadStopReasonTimeout = 4,
    kDownloadStopReasonNoContentLength = 5,
    kDownloadStopReasonContentLengthInvalid = 6,
    kDownloadStopReasonByteRangeInvalid = 7,
    kDownloadStopReasonEnd
} DownloadStopReason;


#ifndef GB
#define GB (1024*1024*1024)
#define MB (1024*1024)
#define KB (1024)
#endif

/**
 * Represents an unset or unknown length.
 */
static const int kLengthUnset = -1;
/**
 * Default offline cache buffer size bytes.
 */
static const long kDefaultBufferSizeBytes = 128 * 1024;

/**
 * Default page size bytes.
 */
static const long kDefaultPageSizeBytes = 4 * 1024;

/**
 * Default maximum whole cached bytes.
 */
static const long kDefaultMaxCacheBytes = 500 * 1024 * 1024;

/**
 * Default maximum single cache file size.
 */
static const long kDefaultMaxCacheFileSize = 2 * 1024 * 1024;

/**
 * Default byte range length.
 */
static const long kDefaultByteRangeLength = 1024 * 1024;
static const long kDefaultFirstByteRangeLength = 3 * 1024 * 1024;

/**
 * A flag indicating whether we will block reads if the cache key is locked. If unset then data is
 * read from upstream if the cache key is locked, regardless of whether the data is cached.
 */
static const int kFlagBlockOnCache = 1 << 0;
/**
 * A flag indicating whether the cache is bypassed following any cache related error. If set
 * then cache related exceptions may be thrown for one cycle of open, read and close calls.
 * Subsequent cycles of these calls will then bypass the cache.
 */
static const int kFlagIgnoreCacheOnError = 1 << 1;

/**
 * A flag indicating that the cache should be bypassed for requests whose lengths are unset.
 */
static const int kFlagIgnoreCacheForUnsetLengthRequest = 1 << 2;

/**
 * A flag indicating that the cache should be bypassed.
 */
static const int kFlagByPassCache = 1 << 3;


static const int kDefaultConnectTimeoutMs = 3000; // 和proxy保持一致
static const int kMinConnectTimeoutMs = 500;
static const int kMaxConnectTimeoutMs = 2 * 60 * 1000;

static const int kDefaultReadTimeoutMs = 5000; //  和proxy实现保持一致
static const int kMinReadTimeoutMs = 500;
static const int kMaxReadTimeoutMs = 2 * 60 * 1000;

static const int kDefaultBufferedDataSourceSizeKb = 64;
static const int kMinBufferedDataSourceSizeKb = 64;
static const int kMaxBufferedDataSourceSizeKb = 10 * 1024;

// 当seek_pos 超过 cur_write_pos_offset_ 一定长度的时候，则认为需要ReOpen。一定要大于等于kMinBufferedDataSourceSizeKb，
// 这里取整数倍,暂时用6倍，和BufferedDataSource的对应值相等
static const int kDefaultSeekReopenThresholdKb = 1024;
static const int kMinSeekReopenThresoldSizeKb  = kMinBufferedDataSourceSizeKb * 5;
static const int kMaxSeekReopenThresoldSizeKb  = 4 * 1024;

static const int kDefaultHttpConnectRetryCount = 0;
static const int kMinHttpConnectRetryCount = 0;
static const int kMaxHttpConnectRetryCount = 10;

static const int kDefaultCurlBufferSizeKb = 800;
static const int kMinCurlBufferSizeKb = 16;
static const int kMaxCurlBufferSizeKb = 10 * 1024;

// 下载回调的频率限制
static const int kDefaultProgressIntervalMs = 50;
static const int kMinProgressIntervalMs = 50;
static const int kMaxProgressIntervalMs = 1000;

// VOD P2SP
static const int kDefaultVodP2spTaskMaxSize = 64 * 1024 * 1024;
static const int kDefaultVodP2spCdnRequestMaxSize = 1024 * 1024;
static const int kDefaultVodP2spCdnRequestInitialSize = 1024 * 1024;
static const int kDefaultVodP2spOnThreshold = 90;
static const int kDefaultVodP2spOffThreshold = 50;
static const int kDefaultVodP2spTaskTimeout = 5000;

// 直播P2SP相关
static const int kDefaultLiveP2spSwitchOnBufferThresholdMs = 3500;
static const int kDefaultLiveP2spSwitchOnBufferHoldThresholdMs = 3000;
static const int kDefaultLiveP2spSwitchOffBufferThresholdMs = 1500;
static const int kDefaultLiveP2spSwitchLagThresholdMs = 500;
static const int kDefaultLiveP2spSwitchMaxCount = 2;
static const int kDefaultLiveP2spSwitchCooldownMs = 5000;
