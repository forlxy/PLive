#import "download/ios/ios_download_task.h"
#import <atomic>
#import <mutex>
#import "download/default_input_stream.h"
#import "utility.h"
#import "xlog/xlog.h"
#import "cache_defs.h"

using namespace kuaishou;
namespace {
static const int kInputStreamCapacity = 100 * 1024; // 100k
static const int kCreated = 1;
static const int kClosed = 2;
static const int kConnected = 3;
static const int kCancelled = 4;
static const int kDefaultTimeout = 5;
}

@implementation NSUrlDownloadTask {
    ConnectionListener* _listener;
    NSURLSession* _session;
    NSOperationQueue* _sessionQueue;
    NSURLSessionDataTask* _dataTask;
    std::shared_ptr<DefaultInputStream> _inputStream;
    std::atomic_int _state;
    std::atomic_bool _pendingClosed;
    std::mutex _callbackMutex;
    std::condition_variable _connectionOpened;
    std::condition_variable _connectionClosed;
    ConnectionInfo _connectionInfo;
    uint64_t _connectStartTime;
    uint64_t _connectedTime;
    uint64_t _downloadedBytes;
    BOOL _paused;
    DownloadOpts _options;
    DataSpec _spec;
    DownloadStopReason _stopReason;
    uint64_t _lastProgressTime;
    uint64_t _lastProgressBytes;
}

- (instancetype)initWithObserver:(ConnectionListener*)listener
                    DownloadOpts:(const DownloadOpts&)options {
    if (self = [super init]) {
        _listener = listener;
        _sessionQueue = [[NSOperationQueue alloc] init];
        _inputStream = std::make_shared<DefaultInputStream>(kInputStreamCapacity);
        _state = kCreated;
        _paused = NO;
        _pendingClosed = NO;
        _options = options;
        _stopReason = kDownloadStopReasonUnknown;
        _lastProgressTime = 0;
        _lastProgressBytes = 0;
    }
    return self;
}

- (ConnectionInfo)makeConnection:(const DataSpec&)dataSpec {
    LOG_DEBUG("[NSUrlDownloadTask] make connection %s", dataSpec.uri.c_str());
    std::unique_lock<std::mutex> lock(_callbackMutex);
    _spec = dataSpec;
    _downloadedBytes = 0;
    _inputStream->Reset();
    NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
    configuration.requestCachePolicy = NSURLRequestReloadIgnoringCacheData;
    configuration.allowsCellularAccess = YES;
    NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:dataSpec.uri.c_str()]];
    // note, ios download task cannot get the true ip address from ios api,
    // in kuaishou's case, the ip is the host of url.
    NSString* host = [url host];
    _connectionInfo.ip = nil;
    _connectionInfo.host = host == nil ? "" : [host UTF8String];
    _connectionInfo.uri = dataSpec.uri;
    NSMutableDictionary* headers = [[NSMutableDictionary alloc] init];
    if (!_options.headers.empty()) {
        auto split_headers = kpbase::StringUtil::Split(_options.headers, "\r\n");
        for (auto& header : split_headers) {
            auto key_value_vec = kpbase::StringUtil::Split(header, ": ");
            if (key_value_vec.size() == 2) {
                auto key = kpbase::StringUtil::Trim(key_value_vec[0]);
                auto value = kpbase::StringUtil::Trim(key_value_vec[1]);
                [headers setObject:[NSString stringWithUTF8String:value.c_str()] forKey:[NSString stringWithUTF8String:key.c_str()]];
                if (key == "host" || key == "Host") {
                    _connectionInfo.host = value;
                }
            }
        }
    }
    if (dataSpec.position != 0 || (dataSpec.length != kLengthUnset && dataSpec.length > 0)) {
        NSString* rangeRequest = @"";
        rangeRequest = [rangeRequest stringByAppendingFormat:@"bytes=%lld-", dataSpec.position];
        if (dataSpec.length != kLengthUnset && dataSpec.length > 0) {
            rangeRequest = [rangeRequest stringByAppendingFormat:@"%lld", dataSpec.position + dataSpec.length - 1];
        }
        [headers setObject:rangeRequest forKey:@"Range"];
    }
    // user agent.
    [headers setObject:_options.user_agent.empty() ? @"ACache" :
                                            [NSString stringWithUTF8String:_options.user_agent.c_str()] forKey : @"User-Agent"];

    configuration.HTTPAdditionalHeaders = headers;
    _session = [NSURLSession sessionWithConfiguration:configuration delegate:self delegateQueue:_sessionQueue];
    _dataTask = [_session dataTaskWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:dataSpec.uri.c_str()]]];
    [_dataTask resume];
    LOG_DEBUG("[NSUrlDownloadTask] after  [_dataTask resume]")
    _connectStartTime = kpbase::SystemUtil::GetCPUTime();
    uint16_t timeout = _options.connect_timeout_ms == kTimeoutUnSet ? kDefaultTimeout * 1000 : _options.connect_timeout_ms;
    _connectionInfo.content_length = kLengthUnset;
    _connectionInfo.response_code = 0;
    _connectionInfo.error_code = kResultExceptionNetDataSourceReadTimeout;
    _connectionInfo.connection_used_time_ms = 0;
    _connectionOpened.wait_for(lock, std::chrono::milliseconds(timeout));
    if (_connectionInfo.error_code == kResultExceptionNetDataSourceReadTimeout) {
        LOG_DEBUG("[NSUrlDownloadTask] make connection kConnectionTimeout ,to cancel .");
        _state = kCancelled;
        [_dataTask cancel];
        [_session invalidateAndCancel];
        _listener->OnConnectionClosed(kDownloadStopReasonTimeout, _connectionInfo, 0, 0);
    } else {
        LOG_DEBUG("[NSUrlDownloadTask] make connection done.");
    }
    return _connectionInfo;
}

- (void)close {
    _pendingClosed = YES;
    _inputStream->Close();
    std::unique_lock<std::mutex> lock(_callbackMutex);
    LOG_VERBOSE("[NSUrlDownloadTask] close()  _listener:%p ", _listener);
    if (_state == kConnected) {
        [_dataTask cancel];
        _connectionClosed.wait(lock);
        _pendingClosed = NO;
    }
    [_session invalidateAndCancel];
    _session = nil;

    LOG_VERBOSE("[NSUrlDownloadTask] closed  _listener:%p ", _listener);
}

- (void)pause {
    if (!_paused) {
        [_dataTask suspend];
        _listener->OnDownloadPaused();
        _paused = YES;
    }
}

- (void)resume {
    if (_paused) {
        [_dataTask resume];
        _listener->OnDownloadResumed();
        _paused = NO;
    }
}


- (shared_ptr<InputStream>)inputStream {
    return _inputStream;
}

- (void)doProgress:(NSData*)data {
    _inputStream->FeedDataSync((uint8_t*)[data bytes], (int32_t)[data length]);
    _downloadedBytes += [data length];
    uint64_t now = kpbase::SystemUtil::GetCPUTime();
    if (now - _lastProgressTime > kDefaultProgressIntervalMs ||
        _downloadedBytes - _lastProgressBytes > kProgressBytesThreshold ||
        _downloadedBytes >= _connectionInfo.content_length) {
        uint64_t bytes = _downloadedBytes >= _connectionInfo.content_length ? _connectionInfo.content_length : _downloadedBytes;
        _listener->OnDownloadProgress(_spec.position + bytes);
        _lastProgressTime = now;
        _lastProgressBytes = _downloadedBytes;
    }
}

- (void)URLSession:(nonnull NSURLSession*)session
              dataTask:(nonnull NSURLSessionDataTask*)dataTask
    didReceiveResponse:(nonnull NSURLResponse*)response
     completionHandler:(nonnull void(^)(NSURLSessionResponseDisposition))completionHandler {
    LOG_DEBUG("[NSUrlDownloadTask] didReceiveResponse");
    std::unique_lock<std::mutex> lock(_callbackMutex);
    if (_state == kCancelled) {
        completionHandler(NSURLSessionResponseCancel);
        return;
    }
    if (_state == kCreated) {
        _connectedTime = kpbase::SystemUtil::GetCPUTime();
        if ([response isKindOfClass:NSHTTPURLResponse.class]) {
            NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
            LOG_DEBUG("[NSUrlDownloadTask] connected %ld, %lld", httpResponse.statusCode, httpResponse.expectedContentLength);
            _connectionInfo.content_length = httpResponse.expectedContentLength;
            _connectionInfo.response_code = (int)httpResponse.statusCode;
            _connectionInfo.connection_used_time_ms = (double)(_connectedTime - _connectStartTime);
            if (httpResponse.statusCode < 200 || httpResponse.statusCode > 299) {
                _connectionInfo.error_code = kHttpInvalidResponseCodeBase - _connectionInfo.response_code;
                if (httpResponse.statusCode == kResponseCodePositionOutOfRange) {
                    _connectionInfo.error_code = kResultExceptionHttpDataSourcePositionOutOfRange;
                }
            }
            _connectionInfo.error_code = kHttpInvalidResponseCodeBase - (int)httpResponse.statusCode;
        }
        completionHandler(NSURLSessionResponseAllow);
        _state = kConnected;
        _connectionOpened.notify_one();
        _listener->OnConnectionOpen(_spec.position, _connectionInfo);
    }

}

- (void)URLSession:(nonnull NSURLSession*)session
          dataTask:(nonnull NSURLSessionDataTask*)dataTask
    didReceiveData:(nonnull NSData*)data {
    LOG_DEBUG("[NSUrlDownloadTask] didReceiveData _listener:%p ", _listener);
    std::unique_lock<std::mutex> lg(_callbackMutex);
    if (_state == kConnected && !_pendingClosed) {
        uint64_t totalBytes = [data length];
        uint64_t pos = 0;
        while (pos < totalBytes && !_pendingClosed) {
            uint64_t progressBytes = totalBytes - pos > kInputStreamCapacity ? kInputStreamCapacity : (totalBytes - pos);
            [self doProgress:[data subdataWithRange:NSMakeRange(pos, progressBytes)]];
            pos += progressBytes;
        }
    }
}

- (void)URLSession:(nonnull NSURLSession*)session
                    task:(nonnull NSURLSessionTask*)task
    didCompleteWithError:(nullable NSError*)error {
    std::unique_lock<std::mutex> lg(_callbackMutex);
    LOG_DEBUG("[[NSUrlDownloadTask] didCompleteWithError");
    if (_state == kCreated && (error == nil || error.code != NSURLErrorCancelled)) {
        // connect failed.
        int errorCode = (int)(error.code < 0 ? error.code : -error.code) + kNsUrlErrorBase;
        _connectionInfo.error_code = errorCode;
        _connectionInfo.content_length = kLengthUnset;
        _connectionInfo.connection_used_time_ms = (int)(kpbase::SystemUtil::GetCPUTime() - _connectStartTime);
        _connectionInfo.response_code = 0;
        _stopReason = kDownloadStopReasonFailed;
        _inputStream->EndOfStream(errorCode);
        // report the response code to up layer using the connection open call.
        _listener->OnConnectionOpen(_spec.position, _connectionInfo);
        _listener->OnConnectionClosed(_stopReason, _connectionInfo, 0, 0);
        _connectionOpened.notify_one();
    } else if (_state == kConnected) {
        // connection closed
        int errorCode = 0;
        if (error != nil && error.code != NSURLErrorCancelled) {
            // error happened.
            errorCode = (int)(error.code < 0 ? error.code : -error.code);
            _stopReason = kDownloadStopReasonFailed;
        } else if (error != nil && error.code == NSURLErrorCancelled) {
            _stopReason = kDownloadStopReasonCancelled;
        } else {
            _stopReason = kDownloadStopReasonFinished;
        }
        // error happened or user closed or end of stream.
        _inputStream->EndOfStream(errorCode);
        _state = kClosed;
        _listener->OnConnectionClosed(_stopReason, _connectionInfo, _downloadedBytes, 0);  // fixme 这里需要算出实际的 transfer_consume_ms
        _connectionClosed.notify_one();
    } else {
        // kClosed -> do nothing
    }
}

@end

namespace kuaishou {
namespace cache {


IosDownloadTask::IosDownloadTask(const DownloadOpts& opts) : options_(opts) {
    download_task_ = [[NSUrlDownloadTask alloc] initWithObserver:this DownloadOpts:options_];
    SetPriority(opts.priority);
}

IosDownloadTask::~IosDownloadTask() {
    LOG_DEBUG("IosDownloadTask::~IosDownloadTask()");
    [download_task_ close];
    download_task_ = nil;
    LOG_DEBUG("IosDownloadTask::~IosDownloadTask() OVER");
}

ConnectionInfo IosDownloadTask::MakeConnection(const DataSpec& spec) {
    return [download_task_ makeConnection:spec];
}

void IosDownloadTask::Pause() {
    [download_task_ pause];
}

void IosDownloadTask::Resume() {
    [download_task_ resume];
}

void IosDownloadTask::Close() {
    LOG_DEBUG("CacheDataSource IosDownloadTask::Close")
    [download_task_ close];
}

std::shared_ptr<InputStream> IosDownloadTask::GetInputStream() {
    return [download_task_ inputStream];
}

void IosDownloadTask::OnConnectionOpen(uint64_t position, const ConnectionInfo& connection_info) {
    NotifyListeners([ = ](DownloadTaskListener * listener) {
        listener->OnConnectionOpen(this, position, connection_info);
    });
}

void IosDownloadTask::OnDownloadProgress(uint64_t position) {
    NotifyListeners([ = ](DownloadTaskListener * listener) {
        listener->OnDownloadProgress(this, position);
    });
}

void IosDownloadTask::OnDownloadPaused() {
    NotifyListeners([ = ](DownloadTaskListener * listener) {
        listener->OnDownloadPaused(this);
    });
}

void IosDownloadTask::OnDownloadResumed() {
    NotifyListeners([ = ](DownloadTaskListener * listener) {
        listener->OnDownloadResumed(this);
    });
}

void IosDownloadTask::OnConnectionClosed(DownloadStopReason reason, const ConnectionInfo& info, uint64_t downloaded_bytes, uint64_t transfer_consume_ms) {
    NotifyListeners([ = ](DownloadTaskListener * listener) {
        listener->OnConnectionClosed(this, info, reason, downloaded_bytes, transfer_consume_ms);
    });
}

PlatformDownloadTask* PlatformDownloadTask::Create(const DownloadOpts& opts) {
    return new IosDownloadTask(opts);
}

} // namespace cache
} // namespace kuaishou

