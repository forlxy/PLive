//
//  KSAwesomeCacheCallback.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/2/1.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "ios_awesome_cache_callback.h"
#import "ios_awesome_cache_callback_c.h"

#import "awesome_cache/utils/macro_util.h"
#import "awesome_cache_callback.h"
#import "KSAwesomeCacheCallbackDelegate.h"
#import "awesome_cache/ios/ios_ac_callback_info.h"

using kuaishou::cache::IOSAcCallbackInfo;

IOSAwesomeCacheCallback::IOSAwesomeCacheCallback(id<KSAwesomeCacheCallbackDelegate> delegate) {
    this->oc_callback_delegate_ = delegate;
}

IOSAwesomeCacheCallback::~IOSAwesomeCacheCallback() {
//     LOG_DEBUG("[IOSAwesomeCacheCallback] ~IOSAwesomeCacheCallback");
}

void IOSAwesomeCacheCallback::onSessionProgress(std::shared_ptr<kuaishou::cache::AcCallbackInfo> info) {
    if (oc_callback_delegate_) {
        std::shared_ptr<IOSAcCallbackInfo> info_ios_impl = std::dynamic_pointer_cast<IOSAcCallbackInfo>(info);
        [oc_callback_delegate_ onSessionProgress:info_ios_impl->oc_object()];
    }
}

void IOSAwesomeCacheCallback::onDownloadFinish(std::shared_ptr<kuaishou::cache::AcCallbackInfo> info) {
    if (oc_callback_delegate_) {
        std::shared_ptr<IOSAcCallbackInfo> info_ios_impl = std::dynamic_pointer_cast<IOSAcCallbackInfo>(info);
        info_ios_impl->SetCdnStatJson(info->GetCdnStatJson());
        [oc_callback_delegate_ onDownloadFinish:info_ios_impl->oc_object()];
    }
}


AwesomeCacheCallback_Opaque AwesomeCacheCallback_Opaque_new(id<KSAwesomeCacheCallbackDelegate> delegate) {
    auto ios_callback = new IOSAwesomeCacheCallback(delegate);
    return ios_callback;
}

void AwesomeCacheCallback_Opaque_delete(AwesomeCacheCallback_Opaque opaque) {
    auto cb = static_cast<IOSAwesomeCacheCallback*>(opaque);
    delete cb;
}
