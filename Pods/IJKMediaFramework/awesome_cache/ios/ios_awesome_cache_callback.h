//
//  ios_awesome_cache_callback.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/10/28.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#pragma once

#import "KSAwesomeCacheCallbackDelegate.h"
#import "awesome_cache/ios/ios_ac_callback_info.h"

using kuaishou::cache::IOSAcCallbackInfo;

class IOSAwesomeCacheCallback: public kuaishou::cache::AwesomeCacheCallback {
  public:
    IOSAwesomeCacheCallback(id<KSAwesomeCacheCallbackDelegate> delegate);

    ~IOSAwesomeCacheCallback();

    void onSessionProgress(std::shared_ptr<kuaishou::cache::AcCallbackInfo> info);

    virtual void onDownloadFinish(std::shared_ptr<kuaishou::cache::AcCallbackInfo> info);

  private:
    id<KSAwesomeCacheCallbackDelegate> oc_callback_delegate_;
};
