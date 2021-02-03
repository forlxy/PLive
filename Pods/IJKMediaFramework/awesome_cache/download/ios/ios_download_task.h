#pragma once
#import <Foundation/Foundation.h>
#import "download/platform_download_task.h"
using namespace kuaishou::cache;

namespace {
class ConnectionListener {
  public:
    virtual ~ConnectionListener() {}
    virtual void OnConnectionOpen(uint64_t position, const ConnectionInfo&) = 0;
    virtual void OnDownloadProgress(uint64_t position) = 0;
    virtual void OnDownloadPaused() = 0;
    virtual void OnDownloadResumed() = 0;
    virtual void OnConnectionClosed(DownloadStopReason, const ConnectionInfo& info, uint64_t, uint64_t) = 0;
};
}

@interface NSUrlDownloadTask : NSObject <NSURLSessionDataDelegate>
- (instancetype)initWithObserver:(ConnectionListener*)listener DownloadOpts:(const DownloadOpts&)options;
- (ConnectionInfo)makeConnection:(const DataSpec&)dataSpec;
- (void)close;
- (void)pause;
- (void)resume;
- (std::shared_ptr<InputStream>)inputStream;
@end

namespace kuaishou {
namespace cache {

class IosDownloadTask : public PlatformDownloadTask, public ConnectionListener {
  public:
    IosDownloadTask(const DownloadOpts& opts);
    virtual ~IosDownloadTask();

    virtual ConnectionInfo MakeConnection(const DataSpec& spec) override;

    virtual void Pause() override;

    virtual void Resume() override;

    virtual void Close() override;

    virtual std::shared_ptr<InputStream> GetInputStream() override;

    virtual  const ConnectionInfo& GetConnectionInfo() override {return dummy_connection_info_;}
  private:
    virtual void OnConnectionOpen(uint64_t position, const ConnectionInfo&) override;
    virtual void OnDownloadProgress(uint64_t position) override;
    virtual void OnDownloadPaused() override;
    virtual void OnDownloadResumed() override;
    virtual void OnConnectionClosed(DownloadStopReason, const ConnectionInfo& info, uint64_t, uint64_t) override;
    const DownloadOpts options_;
    NSUrlDownloadTask* download_task_;
    ConnectionInfo dummy_connection_info_;
};

} // namespace cache
} // namespace kuaishou
