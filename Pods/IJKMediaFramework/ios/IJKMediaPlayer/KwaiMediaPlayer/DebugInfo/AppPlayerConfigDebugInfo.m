//
//  AppPlayerConfigDebugInfo.m
//  IJKMediaFramework
//
//  Created by MarshallShuai on 2018/12/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#import "AppPlayerConfigDebugInfo.h"
#import <MJExtension/MJExtension.h>
#import "DebugInfoUtil.h"
#import "KwaiFFPlayerController.h"
#import "ijkplayer.h"
#import "player_config_debug_info.h"

@interface AppPlayerConfigDebugInfo ()
@property(weak, nonatomic) KwaiFFPlayerController* kwaiPlayer;
@property(nonatomic) PlayerConfigDebugInfo qosDebugInfo;
@end

@implementation AppPlayerConfigDebugInfo

- (NSMutableDictionary*)mj_keyValues {
    return [self mj_keyValuesWithIgnoredKeys:@[ @"kwaiPlayer", @"qosDebugInfo" ]];
}

- (instancetype)init {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:@"-init is not a valid initializer for "
                                          @"the class AppPlayerConfigDebugInfo"
                                 userInfo:nil];
    return nil;
}

- (instancetype)initWith:(KwaiFFPlayerController*)kwaiPlayer {
    self = [super init];
    if (self) {
        _kwaiPlayer = kwaiPlayer;
        PlayerConfigDebugInfo_init(&_qosDebugInfo);
    }
    return self;
}

- (NSString*)getPrettySingleText {
    NSString* Formatter_Title = @">>> %@ <<<\n";
    NSString* Formatter_String = @"%@ : %@\n";
    NSString* Formatter_Int = @"%@ : %d\n";

    NSMutableString* text = [[NSMutableString alloc] initWithFormat:@""];

    // player
    [text appendFormat:Formatter_Title, @"player"];
    [text appendFormat:Formatter_Int, @"max_buffer_dur_ms", _playerMaxBufDurMs];
    [text appendFormat:Formatter_Int, @"start_on_prepared", _playerStartOnPrepared];

    // Hodor
    [text appendFormat:@"\n"];
    [text appendFormat:Formatter_Title, @"Hodor"];
    [text appendFormat:Formatter_Int, @"buffer_ds_size_kb", _cacheBufferDataSourceSizeKb];
    [text appendFormat:Formatter_Int, @"buffer_ds_seek_th_kb", _cacheSeekReopenTHKb];

    [text appendFormat:Formatter_String, @"ds_type", _cacheDataSourceType];
    [text appendFormat:Formatter_Int, @"cache_flags", _cacheFlags];

    [text appendFormat:Formatter_Int, @"progress_cb_ms", _cacheProgressCbIntervalMs];

    [text appendFormat:Formatter_String, @"http_type", _cacheHttpType];
    [text appendFormat:Formatter_String, @"curl_type", _cacheCurlType];
    [text appendFormat:Formatter_Int, @"http_max_retry", _cacheHttpMaxRetryCnt];

    [text appendFormat:Formatter_Int, @"curl_con_timeout_ms", _cacheConnectTimeoutMs];
    [text appendFormat:Formatter_Int, @"curl_read_timeout_ms", _cacheReadTimeoutMs];
    [text appendFormat:Formatter_Int, @"curl_buffer_kb", _cacheCurlBufferSizeKb];
    [text appendFormat:@"socket, orig/cfg/act: %d/%d/%d \n", _cacheSocketOrigKb, _cacheSocketCfgKb,
                       _cacheSocketActKb];
    //        [text appendFormat:Formatter_String, "curl_header", _cacheHeader];
    //        // TODO impl header
    [text appendFormat:Formatter_String, @"curl_user_agent", _cacheUserAgent];

    return text;
}

- (void)refresh {
    struct IjkMediaPlayer* mp = _kwaiPlayer.mediaPlayer;
    if (!mp) {
        return;
    }

    ijkmp_get_player_config_debug_info(mp, &_qosDebugInfo);

    _playerMaxBufDurMs = _qosDebugInfo.playerMaxBufDurMs;
    _playerStartOnPrepared = _qosDebugInfo.playerStartOnPrepared;

    _cacheBufferDataSourceSizeKb = _qosDebugInfo.cacheBufferDataSourceSizeKb;
    _cacheSeekReopenTHKb = _qosDebugInfo.cacheSeekReopenTHKb;

    _cacheDataSourceType = safe_from_c_string(
        _qosDebugInfo
            .cacheDataSourceType);  // [NSString
                                    // stringWithUTF8String:_qosDebugInfo.cacheDataSourceType];

    _cacheFlags = _qosDebugInfo.cacheFlags;

    _cacheProgressCbIntervalMs = _qosDebugInfo.cacheProgressCbIntervalMs;

    _cacheHttpType = safe_from_c_string(_qosDebugInfo.cacheHttpType);  // _qosDebugInfo.?;
    _cacheCurlType = safe_from_c_string(_qosDebugInfo.cacheCurlType);  // _qosDebugInfo.?;
    _cacheHttpMaxRetryCnt = _qosDebugInfo.cacheHttpMaxRetryCnt;

    _cacheConnectTimeoutMs = _qosDebugInfo.cacheConnectTimeoutMs;
    _cacheReadTimeoutMs = _qosDebugInfo.cacheReadTimeoutMs;
    _cacheCurlBufferSizeKb = _qosDebugInfo.cacheCurlBufferSizeKb;
    _cacheSocketOrigKb = _qosDebugInfo.cacheSocketOrigKb;
    _cacheSocketCfgKb = _qosDebugInfo.cacheSocketCfgKb;
    _cacheSocketActKb = _qosDebugInfo.cacheSocketActKb;
    //    _cacheHeader = _qosDebugInfo.?; 暂不维护
    _cacheUserAgent = safe_from_c_string(_qosDebugInfo.cacheUserAgent);  // _qosDebugInfo.?;
}

@end
