#include "cache_manager_impl.h"
#include "download/priority_download_task.h"
#include "cache/simple_cache.h"
#include "cache/least_recently_used_cache_evictor.h"
#include "ac_log.h"
#include "utility.h"
#include "lazy_data_source.h"
#include "cache_factory_impl.h"
#include "cache/cache_util.h"
#include "tasks/export_cached_file_task.h"
#include "default_bandwidth_meter.h"
#include "tasks/offline_cached_file_task.h"
#include "tasks/offline_cache_preload_task.h"
#include "buffered_data_source.h"
#include "live_http_data_source_wrapper.h"

#include "abr/abr_parse_manifest.h"
using namespace kuaishou::abr;

extern "C" {
#include <curl/curl.h>
}

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if TARGET_OS_IPHONE
#include <OpenSSL-Universal/openssl/ssl.h>
#endif

#include <v2/cache/cache_def_v2.h>
#if (ENABLE_CACHE_V2)
#include "v2/cache/cache_v2_settings.h"
#include "v2/cache/cache_content_index_v2.h"
#include "v2/data_source/async_cache_data_source_v2.h"
#include "v2/data_sink/cache_content_v2_data_sink.h"
#include "v2/cache/cache_v2_file_manager.h"
#endif

namespace kuaishou {
namespace cache {

static AwesomeCacheRuntimeInfo ignore_ac_rt_info;

CacheManagerImpl::CacheManagerImpl() :
    inited_(false),
    enabled_(true),
    cache_v2_enabled_(false), // 默认关闭cacheV2
    max_cache_bytes_(kDefaultMaxCacheBytes),
    max_cache_file_size_(kDefaultMaxCacheFileSize),
    evictor_stratergy_(kEvictorLru),
    download_stratergy_(kDownloadSimplePriority),
    enable_detailed_stats_(true),
    has_occur_cache_error_(false),
    proxy_address_(""),
    directory_(/*temp directory*/File(kpbase::SystemUtil::GetDocumentPath() + "/ACache")) {
    // init libcurl
    curl_global_init(CURL_GLOBAL_NOTHING);

    // init download manager.
    download_manager_.reset(DownloadManager::CreateDownloadManager(kDownloadSimplePriority));
    auto download_task_factory = make_shared<DownloadTaskWithPriorityFactory>();
    download_manager_->SetDownloadTaskFactory(download_task_factory);

    http_data_source_factory_ = make_shared<HttpDataSourceFactory>(download_manager_, transfer_listener_);
    file_data_source_factory_ = make_shared<FileDataSourceFactory>(nullptr);
}

CacheManagerImpl::~CacheManagerImpl() {
}

void CacheManagerImpl::SetOptInt(CacheOpt opt, int64_t value) {
    std::lock_guard<std::mutex> lg(mutex_);
    switch (opt) {
        case kMaxCacheBytes: {
            max_cache_bytes_ = value;
            break;
        }
        case kMaxCacheFileSize: {
            max_cache_file_size_ = value;
            break;
        }
        case kCacheEvictorStratergy: {
            evictor_stratergy_ = (CacheEvictorStrategy)value;
            break;
        }
        case kDownloadManagerStratergy: {
            download_stratergy_ = (DownloadStratergy)value;
            break;
        }
        case kEnableDetailedStats: {
            enable_detailed_stats_ = !!value;
            break;
        }
        default: break;
    }
}

void CacheManagerImpl::SetOptStr(CacheOpt opt, const std::string value) {
    std::lock_guard<std::mutex> lg(mutex_);
    switch (opt) {
        case kCacheDir: {
            directory_ = File(value);
            break;
        }
        case kHttpProxyAddress:
            proxy_address_ = value;
            break;
        default: break;
    }
}

int64_t CacheManagerImpl::GetOptInt(CacheOpt opt) {
    std::lock_guard<std::mutex> lg(mutex_);
    switch (opt) {
        case kMaxCacheBytes:
            return max_cache_bytes_;

        case kMaxCacheFileSize:
            return max_cache_file_size_;

        case kCacheEvictorStratergy:
            return evictor_stratergy_ ;

        case kDownloadManagerStratergy:
            return download_stratergy_;

        case kEnableDetailedStats:
            return enable_detailed_stats_;
        default:
            return 0;
    }
}

std::string CacheManagerImpl::GetOptStr(CacheOpt opt) {
    std::lock_guard<std::mutex> lg(mutex_);
    switch (opt) {
        case kCacheDir:
            return directory_.path();

        case kHttpProxyAddress:
            return proxy_address_;

        default:
            return "";
    }
}

bool CacheManagerImpl::_InitOnceIfEnabled() {
    if (!enabled_) {
        LOG_DEBUG("[CacheManagerImpl::_InitOnceIfEnabled], not enabled, return false");
        return false;
    }

    if (inited_) {
        return true;
    }


#if (ENABLE_CACHE_V2)
    CacheV2FileManager::GetMediaDirManager()->Index()->Load();
    CacheV2FileManager::GetMediaDirManager()->RestoreFileInfo();
    CacheV2FileManager::GetResourceDirManager()->Index()->Load();
    CacheV2FileManager::GetResourceDirManager()->RestoreFileInfo();
#endif

    LOG_DEBUG("[CacheManager]***********************init begin********************************");
    LOG_DEBUG("[CacheManager] cache dir %s", directory_.path().c_str());
    LOG_DEBUG("[CacheManager] max cache storage in bytes %lld", max_cache_bytes_);
    LOG_DEBUG("[CacheManager] max cache file span in bytes %lld", max_cache_file_size_);
    LOG_DEBUG("[CacheManager] cache evictor stratergy %s", evictor_stratergy_ == kEvictorLru ? "kEvictorLru" : "kEvictorNoOp");
    LOG_DEBUG("[CacheManager] cache download stratergy %s", download_stratergy_ == kDownloadSimplePriority ? "kDownloadSimplePriority" : "kDownloadNoOp");
    LOG_DEBUG("[CacheManager]***********************init done********************************");

    // init cache
    index_ = std::make_shared<CachedContentIndex>(directory_);
    cache_.reset(CacheFactoryImpl(directory_, index_, evictor_stratergy_, max_cache_bytes_).CreateCache());

    // bandwidth meter
    // transfer_listener_ = std::make_shared<DefaultBandwidthMeter<HttpDataSource> >(nullptr);

    // init data source factories
    cache_data_source_factory_ = make_shared<CacheDataSourceFactory>(cache_.get(),
                                                                     http_data_source_factory_,
                                                                     file_data_source_factory_,
                                                                     max_cache_file_size_);
    Stats::EnableStats(enable_detailed_stats_);
    inited_ = true;

    LOG_DEBUG("[CacheManager]*******************init cache done******************************");
    return true;
}

void CacheManagerImpl::Init() {
    std::lock_guard<std::mutex> lg(mutex_);

    _InitOnceIfEnabled();
}

void CacheManagerImpl::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lg(mutex_);
    enabled_ = enabled;
}

bool CacheManagerImpl::IsEnabled() {
    std::lock_guard<std::mutex> lg(mutex_);
    return enabled_;
}

bool CacheManagerImpl::Initialized() {
    std::lock_guard<std::mutex> lg(mutex_);
    return inited_;
}

std::unique_ptr<DataSource> CacheManagerImpl::CreateDataSource(DataSourceOpts& opts,
                                                               shared_ptr<CacheSessionListener> listener,
                                                               AwesomeCacheRuntimeInfo* ac_rt_info,
                                                               bool* seekable) {
    bool _unused;
    if (!seekable)
        seekable = &_unused;

    if (ac_rt_info) {
        ac_rt_info->cache_applied_config.data_source_type = opts.type;
        ac_rt_info->cache_applied_config.cache_flags = opts.cache_flags;
        ac_rt_info->cache_applied_config.buffered_type = opts.buffered_datasource_type;
        ac_rt_info->cache_applied_config.upstream_type = opts.download_opts.upstream_type;
        ac_rt_info->cache_applied_config.curl_type = opts.download_opts.curl_type;
    }
    // for live, do not init cache manager to optimize first screen time
    // also, locking is not required here
    // Return plain http datasource without cache and seek
    if (opts.type == kDataSourceTypeLiveNormal ||
        opts.type == kDataSourceTypeLiveAdaptive) {
        *seekable = false;

        DownloadOpts download_opts_live = opts.download_opts;
        download_opts_live.is_live = true;
        return std::unique_ptr<DataSource>(new LiveHttpDataSourceWrapper(
                                               std::unique_ptr<HttpDataSource>(
                                                   http_data_source_factory_->CreateDataSource(download_opts_live, ac_rt_info, opts.type))));
    }

    std::lock_guard<std::mutex> lg(mutex_);

    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return nullptr;
    }

    *seekable = true;

#if (ENABLE_CACHE_V2)
    if (cache_v2_enabled_) {
        if (opts.type == kDataSourceTypeAsyncV2
            || opts.type == kDataSourceTypeDefault
            || opts.type == kDataSourceTypeAsyncDownload) {
            // sync和async都变成asyncV2
            if (ac_rt_info) {
                // override buffered_type
                ac_rt_info->cache_applied_config.data_source_type = kDataSourceTypeAsyncV2;
            }
            opts.type = kDataSourceTypeAsyncV2;
            auto data_source = std::unique_ptr<DataSource>(
                                   cache_data_source_factory_->CreateCacheDataSource(opts, listener, ac_rt_info));

            return std::unique_ptr<DataSource>(new BufferedDataSource(opts.buffered_datasource_size_kb * 1024,
                                                                      opts.seek_reopen_threshold_kb * 1024,
                                                                      std::move(data_source),
                                                                      opts.context_id,
                                                                      ac_rt_info));
        }
    }
#endif

    // use old sync/async as usual
    auto data_source = std::unique_ptr<DataSource>(cache_data_source_factory_->CreateCacheDataSource(opts,
                                                   listener, ac_rt_info));
    return std::unique_ptr<DataSource>(new BufferedDataSource(opts.buffered_datasource_size_kb * 1024,
                                                              opts.seek_reopen_threshold_kb * 1024,
                                                              std::move(data_source),
                                                              opts.context_id,
                                                              ac_rt_info));


}

#define CONTEXT_ID_EXPORT_TASK -2
#define CONTEXT_ID_OFFLINE_CACHE_FILE_TASK -3
#define CONTEXT_ID_OFFLINE_CACHE_PRELOAD_TASK -4

std::unique_ptr<Task> CacheManagerImpl::CreateExportCachedFileTask(const std::string& uri, const std::string& key,
                                                                   DataSourceOpts& options, const std::string& file_name, TaskListener* listener) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return nullptr;
    }


    DataSpec spec = DataSpec().WithUri(uri).WithKey(key);

#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        options.type = kDataSourceTypeAsyncV2;
        auto async_v2_data_source = std::unique_ptr<BufferedDataSource>(static_cast<BufferedDataSource*>(
                                                                            cache_data_source_factory_->CreateCacheDataSource(options, nullptr, &ignore_ac_rt_info)));
        return std::unique_ptr<Task>(new ExportCachedFileTask(std::move(async_v2_data_source), spec, file_name, listener));
    }
#endif
    auto data_source = std::unique_ptr<DataSource>(cache_data_source_factory_->CreateCacheDataSource(options, nullptr, &ignore_ac_rt_info));
    auto buffered_data_source = std::unique_ptr<BufferedDataSource>(new BufferedDataSource(kDefaultBufferedDataSourceSizeKb * 1024,
                                                                    kDefaultSeekReopenThresholdKb * 1024,
                                                                    std::move(data_source),
                                                                    CONTEXT_ID_EXPORT_TASK,
                                                                    &ignore_ac_rt_info));
    return std::unique_ptr<Task>(new ExportCachedFileTask(std::move(buffered_data_source), spec, file_name, listener));
}

std::unique_ptr<Task> CacheManagerImpl::CreateOfflineCachePreloadManifestTask(OfflienCacheDataSpec data_spec,
                                                                              OfflienCacheVodAdaptiveInit vod_adaptive_init,
                                                                              DataSourceOpts& options,
                                                                              TaskListener* listener) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return nullptr;
    }

    // parse manifest
    int ret = 0;
    shared_ptr<kuaishou::abr::AbrParseManifest> abrParser = shared_ptr<kuaishou::abr::AbrParseManifest>(new kuaishou::abr::AbrParseManifest());
    abrParser->Init(vod_adaptive_init.dev_res_width, vod_adaptive_init.dev_res_heigh, vod_adaptive_init.net_type,
                    vod_adaptive_init.low_device, vod_adaptive_init.signal_strength, 0);
    if (!vod_adaptive_init.rate_config.empty()) {
        ret = abrParser->ParserRateConfig(vod_adaptive_init.rate_config);
        if (ret) {
            LOG_ERROR("[%s] parser rate config failed", __func__);
            return nullptr;
        }
    }

    ret = abrParser->ParserVodAdaptiveManifest(data_spec.url);
    if (ret) {
        LOG_ERROR("[%s] parser manifest failed", __func__);
        return nullptr;
    }

    int index = abrParser->AbrEngienAdaptInit();
    std::string adaptive_url = abrParser->GetUrl(index);
    std::string adaptive_key = abrParser->GetKey(index);
    std::string adaptive_host = abrParser->GetHost(index);
    int download_len = abrParser->GetDownloadLen(index);
    if (download_len != UNKNOWN_DOWNLOAD_LEN) {
        data_spec.len = download_len;
    } else {
        LOG_ERROR("[%s] can't get download len from manifest", __func__);
        if (data_spec.len <= 0 && data_spec.durMs > 0) {
            int avg_bitrate = abrParser->GetAvgBitrate(index);
            download_len = (int)(avg_bitrate * data_spec.durMs / 8);
            data_spec.len = download_len;
        }
    }

    return CreateOfflineCachePreloadTask(adaptive_url, adaptive_key, adaptive_host, options, listener, data_spec.pos, data_spec.len);
}


std::unique_ptr<Task> CacheManagerImpl::CreateOfflineCachePreloadFileTask(OfflienCacheDataSpec data_spec,
                                                                          DataSourceOpts& options,
                                                                          TaskListener* listener) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return nullptr;
    }

    return CreateOfflineCachePreloadTask(data_spec.url, data_spec.key, data_spec.host, options, listener, data_spec.pos, data_spec.len);
}

std::unique_ptr<Task> CacheManagerImpl::CreateOfflineCachePreloadTask(const std::string& url, const std::string& key,
                                                                      const std::string& host,
                                                                      DataSourceOpts& options,
                                                                      TaskListener* listener,
                                                                      int64_t pos, int64_t len) {
    options.download_opts.priority = kPriorityHigh;
    options.enable_vod_adaptive = 0;
    options.download_opts.progress_cb_interval_ms = 50;
    options.cache_flags = 1;
    if (!host.empty()) {
        options.download_opts.headers = "Host:" + host;
    }

    if (options.type == kDataSourceTypeAsyncDownload) {
        options.download_opts.curl_type = kCurlTypeAsyncDownload;

        if (len < kDefaultFirstByteRangeLength) {
            options.first_byte_range_size = len;
            options.byte_range_size = kDefaultByteRangeLength;
        } else {
            options.first_byte_range_size = kDefaultFirstByteRangeLength;
            options.byte_range_size = kDefaultByteRangeLength;
        }
    }

    // download file
    auto offline_preload_lister = shared_ptr<CacheSessionListener>(new OfflineCachePreloadLister(listener));
    auto data_source = std::unique_ptr<DataSource>(cache_data_source_factory_->CreateCacheDataSource(options, std::move(offline_preload_lister), &ignore_ac_rt_info));
    auto buffered_data_source = std::unique_ptr<BufferedDataSource>(new BufferedDataSource(kDefaultBufferedDataSourceSizeKb * 1024,
                                                                    kDefaultSeekReopenThresholdKb,
                                                                    std::move(data_source),
                                                                    CONTEXT_ID_OFFLINE_CACHE_PRELOAD_TASK,
                                                                    &ignore_ac_rt_info));
    DataSpec spec = DataSpec().WithUri(url).WithKey(key).WithPosition(pos);
    return std::unique_ptr<Task>(new OfflineCachePreloadTask(std::move(buffered_data_source), spec, options.type, listener, pos, len));
}


std::unique_ptr<Task> CacheManagerImpl::CreateOfflineCachedFileTask(const std::string& uri, const std::string& key,
                                                                    DataSourceOpts& options, TaskListener* listener) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return nullptr;
    }

    auto offlinelister = shared_ptr<CacheSessionListener>(new OfflineCachedLister(listener));


    DataSpec spec = DataSpec().WithUri(uri).WithKey(key);
#if ENABLE_CACHE_V2
    if (CacheManager::GetInstance()->IsCacheV2Enabled()) {
        options.type = kDataSourceTypeAsyncV2;
        auto async_v2_data_source = std::unique_ptr<DataSourceSeekable>(
                                        dynamic_cast<DataSourceSeekable*>(cache_data_source_factory_->CreateCacheDataSource(options, std::move(offlinelister), &ignore_ac_rt_info)));
        return std::unique_ptr<Task>(
                   new OfflineCachedFileTask(std::move(async_v2_data_source), spec, kDataSourceTypeAsyncV2, listener));
    }
#endif
    if (options.type == kDataSourceTypeAsyncV2) {
        LOG_WARN("[CacheManagerImpl::CreateOfflineCachedFileTask]IsCacheV2Enabled not true, fallback to syncDataSource");
        options.type = kDataSourceTypeDefault;
    }
    auto data_source = std::unique_ptr<DataSource>(cache_data_source_factory_->CreateCacheDataSource(options, std::move(offlinelister), &ignore_ac_rt_info));
    auto buffered_data_source = std::unique_ptr<BufferedDataSource>(new BufferedDataSource(kDefaultBufferedDataSourceSizeKb * 1024,
                                                                    kDefaultSeekReopenThresholdKb,
                                                                    std::move(data_source),
                                                                    CONTEXT_ID_OFFLINE_CACHE_FILE_TASK,
                                                                    &ignore_ac_rt_info));
    return std::unique_ptr<Task>(new OfflineCachedFileTask(std::move(buffered_data_source), spec, options.type, listener));
}

bool CacheManagerImpl::ImportFileToCache(const std::string& path, const std::string& key) {
    {
        std::lock_guard<std::mutex> lg(mutex_);
        if (!_InitOnceIfEnabled()) {
            LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
            return false;
        }
    }

    if (path.empty() || key.empty()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCache] invalid params, path:%s, key:%s", path.c_str(), key.c_str());
        return false;
    }

    DataSpec fileSpec;
    fileSpec.WithUri(path).WithKey(key);

    const int TMP_READ_BUF_MAX_LEN = 1024 * 20;
    uint8_t buf[TMP_READ_BUF_MAX_LEN];
    std::shared_ptr<FileDataSource> fileSource = std::make_shared<FileDataSource>();
    if (fileSource == nullptr) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCache] create FileDataSource fail");
        return false;
    }

    std::shared_ptr<DataSink> cacheDataSink = std::unique_ptr<DataSink>(cache_data_source_factory_->CreateCacheDataSink(&ignore_ac_rt_info));
    if (fileSource == nullptr) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCache] create CreateCacheDataSink fail");
        return false;
    }

    int64_t fileSize = fileSource->Open(fileSpec);
    if (fileSize < 0) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCache] open file fail, ret:%lld", fileSize);
        return false;
    }

    DataSpec sinkSpec = fileSpec;
    sinkSpec.WithLength(fileSize);
    int64_t ret  = cacheDataSink->Open(sinkSpec);

    if (ret < 0) {
        fileSource->Close();
        if (ret == kResultContentAlreadyCached) {
            LOG_DEBUG("[CacheManagerImpl::ImportFileToCache] cacheDataSink->Open ret: kResultContentAlreadyCached");
            return true;
        } else {
            LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCache] cacheDataSink->Open fail, ret:%d", ret);
            return false;
        }
    }

    int64_t read_len = 0;
    bool return_ret = true;

    while ((read_len = fileSource->Read(buf, 0, TMP_READ_BUF_MAX_LEN)) > 0) {
        if (ret < 0 && ret != kResultEndOfInput) {
            fileSource->Close();
            cacheDataSink->Close();
            LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCache] cacheDataSink write failed %d", ret);
            return_ret = false;
            goto return_on_release;
        }

        ret = cacheDataSink->Write(buf, 0, read_len);
        if (ret < 0 && ret != kResultContentAlreadyCached) {
            return_ret = false;
            goto return_on_release;
        }
    }

    return_ret = cache_->SetContentLength(key, fileSize);

return_on_release:
    fileSource->Close();
    cacheDataSink->Close();
    LOG_DEBUG("[CacheManagerImpl::ImportFileToCache] complete wit ret:%d", return_ret);
    return return_ret;
}

void CacheManagerImpl::ClearCacheForKey(const std::string& key) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return;
    }
    auto spans = cache_->GetCachedSpans(key);
    for (auto& span : spans) {
        cache_->RemoveSpan(span);
    }
}

void CacheManagerImpl::ClearCacheDir() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return;
    }
    // todo pause all and kill all download task when we have offline cache task feature
    cache_->ClearCacheSpace();
}


bool CacheManagerImpl::IsFullyCached(const std::string& key) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return false;
    }
    return cache_->IsFullyCached(key);
}

int64_t CacheManagerImpl::GetCachedBytes() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return 0;
    }
    return cache_->GetCacheSpace();
}

int64_t CacheManagerImpl::GetTotalBytesForKey(const std::string& key) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return 0;
    }
    return cache_->GetContentLength(key);
}

int64_t CacheManagerImpl::GetCachedBytesForKey(const std::string& key) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (!_InitOnceIfEnabled()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl]->%s(), not Initialized or enabled", __func__);
        return 0;
    }
    return cache_->GetContentCachedBytes(key);
}

bool CacheManagerImpl::hasOccurCacheError() {
    return has_occur_cache_error_;
}

void CacheManagerImpl::onCacheErrorOccur() {
    has_occur_cache_error_ = true;
}


CacheManager* CacheManager::instance_;
std::mutex CacheManager::mutex_;

CacheManager* CacheManager::GetInstance() {
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance_ == nullptr) {
            instance_ = new CacheManagerImpl();
#if TARGET_OS_IPHONE
            SSL_library_init();
#endif
        }
    }
    return instance_;
}

#pragma CacheV2 related
void CacheManagerImpl::SetCacheV2Enabled(bool enabled) {
#if ENABLE_CACHE_V2
    cache_v2_enabled_ = enabled;
    LOG_DEBUG("[CacheManagerImpl::SetCacheV2Enabled] :%d", enabled);
#endif
}

bool CacheManagerImpl::IsCacheV2Enabled() {
#if ENABLE_CACHE_V2
    return cache_v2_enabled_;
#else
    return false;
#endif
}

bool CacheManagerImpl::ImportFileToCacheV2(const std::string& path, const std::string& key) {
#if ENABLE_CACHE_V2
    {
        std::lock_guard<std::mutex> lg(mutex_);
        if (!_InitOnceIfEnabled()) {
            LOG_ERROR_DETAIL("[CacheManagerImpl:ImportFileToCacheV2], not Initialized or enabled");
            return false;
        }
    }

    if (path.empty() || key.empty()) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCacheV2] invalid params, path:%s, key:%s", path.c_str(), key.c_str());
        return false;
    }

//    const int TMP_READ_BUF_MAX_LEN = 20*1024;
    const int TMP_READ_BUF_MAX_LEN = 1024 * 1024 + 1024;
    uint8_t buf[TMP_READ_BUF_MAX_LEN];

    std::shared_ptr<FileDataSource> file_data_source = std::make_shared<FileDataSource>();
    if (file_data_source == nullptr) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCacheV2] create FileDataSource fail");
        return false;
    }
    DataSpec spec = DataSpec().WithUri(path).WithKey(key);
    int64_t file_len = file_data_source->Open(spec);
    if (file_len < 0) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCacheV2] open FileDataSource fail, error:%d", (int)file_len);
        return false;
    }

    auto cache_content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", key, true);

    if (cache_content->GetContentLength() >  0 && cache_content->GetContentLength() != file_len) {
        LOG_WARN("[CacheManagerImpl::ImportFileToCacheV2] cache_content's length(%lld) != input file source len(%lld), remove old content files",
                 cache_content->GetContentLength(), file_len);
        cache_content->DeleteScopeFiles();
        CacheV2FileManager::GetMediaDirManager()->Index()->RemoveCacheContent(cache_content, true);
        cache_content = CacheV2FileManager::GetMediaDirManager()->Index()->GetCacheContent("", key, false);
    }
    cache_content->SetContentLength(file_len);
    CacheV2FileManager::GetMediaDirManager()->Index()->PutCacheContent(cache_content);

    auto cache_content_sink = std::make_shared<CacheContentV2WithScopeDataSink>(cache_content);
    if (cache_content_sink == nullptr) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCacheV2] create CreateCacheDataSink fail");
        return false;
    }

    DataSpec sink_spec;
    auto ret  = cache_content_sink->Open(sink_spec);
    if (ret < 0) {
        LOG_ERROR_DETAIL("[CacheManagerImpl::ImportFileToCacheV2] sink->Open fail, error:%d", (int)ret);
        return false;
    }

    int64_t total_import_len = 0;
    int64_t write_ret = 0, read_ret = 0;

    while (total_import_len < file_len) {
        read_ret = file_data_source->Read(buf, 0, TMP_READ_BUF_MAX_LEN);
        if (read_ret <= 0) {
            LOG_ERROR("[CacheManagerImpl::ImportFileToCacheV2]file_data_source->Read fail, error:%d", (int)read_ret);
            return false;
        }

        write_ret = cache_content_sink->Write(buf, 0, read_ret);
        if (write_ret <= 0) {
            LOG_ERROR("[CacheManagerImpl::ImportFileToCacheV2]cache_content_sink->Write fail, error:%d", (int)write_ret);
            return false;
        }
        if (read_ret != write_ret) {
            LOG_ERROR("[CacheManagerImpl::ImportFileToCacheV2]inner error, read_ret(%lld)!=write_len(%lld)",
                      read_ret, write_ret);
            return false;
        }
        total_import_len += read_ret;
    }
    cache_content_sink->Close();


    LOG_DEBUG("[CacheManagerImpl::ImportFileToCacheV2] SUCCESS, total_import_len:%lld, file_len:%lld", total_import_len, file_len);
    return true;
#else
    return false;
#endif  // ENABLE_CACHE_V2
}

}
}
