//
//  KSAwesomeCacheCallbackDelegate.h
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/2/1.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KSAcCallbackInfo.h"

@protocol KSAwesomeCacheCallbackDelegate <NSObject>

- (void)onSessionProgress:(KSAcCallbackInfo *)info;
- (void)onDownloadFinish:(KSAcCallbackInfo *)info;

@end
