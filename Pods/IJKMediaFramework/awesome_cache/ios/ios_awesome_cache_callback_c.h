//
//  Header.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/9/3.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "KSAwesomeCacheCallbackDelegate.h"
#import "awesome_cache_callback_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 这个接口会生成一个 AwesomeCacheCallback 的新对象，以指针形式返回
 * 约定：外部的类需要负责释放
 */
AwesomeCacheCallback_Opaque AwesomeCacheCallback_Opaque_new(id<KSAwesomeCacheCallbackDelegate> delegate);

#ifdef __cplusplus
}
#endif
