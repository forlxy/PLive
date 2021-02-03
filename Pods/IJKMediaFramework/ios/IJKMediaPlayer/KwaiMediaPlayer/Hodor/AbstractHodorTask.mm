//
//  AbstractHodorTask.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2019/11/14.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#import "AbstractHodorTask.h"
#import "AbstractHodorTask+private.h"
#import "HodorConfig.h"
#import "utils/macro_util.h"
#import "hodor_log_switch.h"
#import "ac_log.h"

USING_HODOR_NAMESPAE

@implementation AbstractHodorTask

- (instancetype)init
{
    self = [super init];
    if (self) {
        _mainPriority = High;
        _subPriority = High;
        _maxSpeedKbps = -1; // 默认不限速
        _connectTimeoutMs = DEFAULT_CONNECT_TIMEOUT_MS;
    }
    return self;
}

void ExtractValueIntoDownloadOpts(AbstractHodorTask *task, DownloadOpts &opts) {
    // opts.upstream_type = task.upstreamType; // ios暂不支持upStream,先注释
    opts.connect_timeout_ms = task.connectTimeoutMs;
    opts.max_speed_kbps = task.maxSpeedKbps;

#if (VERBOSE_HODOR_TASK_CONFIG)
    LOG_VERBOSE("[AbstractHodorTaskHolder::ExtractValueIntoDownloadOpts] upstream_type:%d, connect_timeout_ms:%dms, max_speed_kbps:%d",
                opts.upstream_type, opts.connect_timeout_ms, opts.max_speed_kbps);
#endif
}

@end
