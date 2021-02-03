//
//  KSCacheSessionListenerBridge.h
//  KSYPlayerCore
//
//  Created by 帅龙成 on 21/12/2017.
//  Copyright © 2017 kuaishou. All rights reserved.
//

#import "IJKFFMoviePlayerController.h"
#import "cache_session_listener_c.h"

CCacheSessionListener* CCacheSessionListener_create(IJKFFMoviePlayerController* controller);
void CCacheSessionListener_freep(CCacheSessionListener** listener);
