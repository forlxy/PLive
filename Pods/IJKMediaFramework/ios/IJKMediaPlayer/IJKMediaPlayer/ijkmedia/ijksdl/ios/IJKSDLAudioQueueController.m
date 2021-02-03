/*
 * IJKSDLAudioQueueController.m
 *
 * Copyright (c) 2013-2014 Zhang Rui <bbcallen@gmail.com>
 *
 * based on https://github.com/kolyvan/kxmovie
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#import "IJKSDLAudioQueueController.h"
#import "IJKSDLAudioKit.h"
#import "ijksdl_log.h"

#import <AVFoundation/AVFoundation.h>

#define kIJKAudioQueueNumberBuffers (3)
#define kSpeaderDelaySec (0.15)
#define kBluetoothDelaySec (0.3)

@implementation IJKSDLAudioQueueController {
    AudioQueueRef _audioQueueRef;
    AudioQueueBufferRef _audioQueueBufferRefArray[kIJKAudioQueueNumberBuffers];
    BOOL _isPaused;
    BOOL _isStopped;

    volatile BOOL _isAborted;
    NSLock* _lock;
    /** KingSoft's code begin **/
    AudioStreamBasicDescription _streamDescription;
    CFAbsoluteTime _startTime;
    /** KingSoft's code end **/

    // Kwai
    BOOL _isClear;
    double _deviceAoutDelaySec;

    float _playbackVolume;
    float _playbackRate;  //值范围：0.2-2
}

+ (NSArray<AVAudioSessionPortDescription*>*)getAudioOutputPorts {
    AVAudioSession* audioSession = [AVAudioSession sharedInstance];
    NSArray<AVAudioSessionPortDescription*>* outputPorts = [[audioSession currentRoute] outputs];
    return outputPorts;
}

+ (BOOL)isConnectHeadphones {
    for (AVAudioSessionPortDescription* port in [self getAudioOutputPorts]) {
        if ([[port portType] isEqualToString:AVAudioSessionPortHeadphones]) {
            return true;
        }
    }
    return false;
}

+ (BOOL)isConnectBluetoothHeadset {
    for (AVAudioSessionPortDescription* port in [self getAudioOutputPorts]) {
        if ([@[ AVAudioSessionPortBluetoothA2DP, AVAudioSessionPortBluetoothHFP ]
                containsObject:[port portType]]) {
            return true;
        }
    }
    return false;
}

- (BOOL)initAudioQueue {
    if (_audioQueueRef) {
        AudioQueueStop(_audioQueueRef, true);
        AudioQueueDispose(_audioQueueRef, true);
    }
    _audioQueueRef = nil;
    AudioQueueRef audioQueueRef;
    OSStatus status =
        AudioQueueNewOutput(&_streamDescription, IJKSDLAudioQueueOuptutCallback,
                            (__bridge void*)self, NULL, kCFRunLoopCommonModes, 0, &audioQueueRef);
    if (status != noErr) {
        ALOGE("AudioQueue: AudioQueueNewOutput failed (%d)\n", (int)status);
        return false;
    }

    UInt32 propValue = 1;
    AudioQueueSetProperty(audioQueueRef, kAudioQueueProperty_EnableTimePitch, &propValue,
                          sizeof(propValue));
    propValue = 1;
    AudioQueueSetProperty(audioQueueRef, kAudioQueueProperty_TimePitchBypass, &propValue,
                          sizeof(propValue));
    propValue = kAudioQueueTimePitchAlgorithm_Spectral;
    AudioQueueSetProperty(audioQueueRef, kAudioQueueProperty_TimePitchAlgorithm, &propValue,
                          sizeof(propValue));

    status = AudioQueueStart(audioQueueRef, NULL);
    if (status != noErr) {
        ALOGE("AudioQueue: AudioQueueStart failed (%d)\n", (int)status);
        AudioQueueDispose(audioQueueRef, true);
        return false;
    }

    for (int i = 0; i < kIJKAudioQueueNumberBuffers; i++) {
        status = AudioQueueAllocateBuffer(audioQueueRef, _spec.size, &_audioQueueBufferRefArray[i]);
        if (status != noErr) {
            ALOGE("AudioQueue: AudioQueueAllocateBuffer failed (%d)\n", (int)status);
            AudioQueueDispose(audioQueueRef, true);
            return false;
        }
        _audioQueueBufferRefArray[i]->mAudioDataByteSize = _spec.size;
        memset(_audioQueueBufferRefArray[i]->mAudioData, 0, _spec.size);
        AudioQueueEnqueueBuffer(audioQueueRef, _audioQueueBufferRefArray[i], 0, NULL);
    }

    _audioQueueRef = audioQueueRef;

    return true;
}

- (id)initWithAudioSpec:(const SDL_AudioSpec*)aSpec {
    self = [super init];
    if (self) {
        if (aSpec == NULL) {
            self = nil;
            return nil;
        }
        _spec = *aSpec;
        _isClear = false;
        _playbackVolume = -1.0f;
        _playbackRate = -1.0f;
        _audioQueueRef = nil;

        if (aSpec->format != AUDIO_S16SYS) {
            NSLog(@"aout_open_audio: unsupported format %d\n", (int)aSpec->format);
            return nil;
        }

        if (aSpec->channels > 2) {
            NSLog(@"aout_open_audio: unsupported channels %d\n", (int)aSpec->channels);
            return nil;
        }

        /* Get the current format */
        IJKSDLGetAudioStreamBasicDescriptionFromSpec(&_spec, &_streamDescription);

        SDL_CalculateAudioSpec(&_spec);

        if (_spec.size == 0) {
            NSLog(@"aout_open_audio: unexcepted audio spec size %u", _spec.size);
            return nil;
        }

        /* Set the desired format */
        if (![self initAudioQueue]) {
            self = nil;
            return nil;
        }

        if ([IJKSDLAudioQueueController isConnectBluetoothHeadset]) {
            _deviceAoutDelaySec = kBluetoothDelaySec;
        } else {
            _deviceAoutDelaySec = kSpeaderDelaySec;
        }

        _isStopped = NO;

        _lock = [[NSLock alloc] init];
    }
    return self;
}

- (void)dealloc {
    [self close];
}

//- (v)

/**
 *  在同框拍摄和K歌应用中都遇到暂停播放器后，进入录制界面，放弃录制返回，
 *  此时再播放没有声音的问题，而且此时AudioQueue不会报错。
 *  在这种情况下，只有重新初始化AudioQueue才能恢复。
 */
- (void)play {
    if (!_audioQueueRef) return;

    self.spec.callback(self.spec.userdata, NULL, 0);

    @synchronized(_lock) {
        NSError* error = nil;
        if (NO == [[AVAudioSession sharedInstance] setActive:YES error:&error]) {
            NSLog(@"[%s] AudioQueue: AVAudioSession.setActive(YES) failed: %@\n", __func__,
                  error ? [error localizedDescription] : @"nil");
            if (![self initAudioQueue]) {
                ALOGE("[%s] reinit AudioQueue failed\n", __func__);
                return;
            }
        }

        OSStatus status = AudioQueueStart(_audioQueueRef, NULL);
        if (status != noErr) {
            NSLog(@"[%s]: AudioQueueStart failed (%d)\n", __func__, (int)status);
        }

        // [BUG] 解决同框拍摄调速失败的问题。
        if (_isPaused || _isClear) {
            if (_playbackVolume > -0.000001) {
                [self setPlaybackVolume:_playbackVolume];
            }
            if (_playbackRate > -0.000001) {
                [self setPlaybackRate:_playbackRate];
            }
        }
        _isPaused = NO;
    }
}

- (void)pause {
    if (!_audioQueueRef) return;

    @synchronized(_lock) {
        if (_isStopped) return;

        _isPaused = YES;
        OSStatus status = AudioQueuePause(_audioQueueRef);
        if (status != noErr) {
            NSLog(@"AudioQueue: AudioQueuePause failed (%d)\n", (int)status);
        } else {
            // clear audio buffer in AudioQueue to avoid noise
            for (int i = 0; i < kIJKAudioQueueNumberBuffers; i++) {
                memset(_audioQueueBufferRefArray[i]->mAudioData, 0, _spec.size);
            }
        }
    }
}

- (void)flush {
    if (!_audioQueueRef) return;

    @synchronized(_lock) {
        if (_isStopped) return;

        AudioQueueFlush(_audioQueueRef);
    }
}

- (void)clear {
    if (!_audioQueueRef) return;

    @synchronized(_lock) {
        if (_isStopped) return;

        AudioQueueFlush(_audioQueueRef);
        // clear audio buffer in AudioQueue to avoid noise
        for (int i = 0; _audioQueueRef && i < kIJKAudioQueueNumberBuffers; i++) {
            memset(_audioQueueBufferRefArray[i]->mAudioData, 0, _spec.size);
        }
        _isClear = true;
    }
}

- (void)stop {
    if (!_audioQueueRef) return;

    @synchronized(_lock) {
        if (_isStopped) return;

        _isStopped = YES;
    }

    // do not lock AudioQueueStop, or may be run into deadlock
    AudioQueueStop(_audioQueueRef, true);
    AudioQueueDispose(_audioQueueRef, true);
}

- (void)close {
    [self stop];
    _audioQueueRef = nil;
}

- (void)setMute:(BOOL)shouldMute {
    @synchronized(_lock) {
        _mute = shouldMute;
    }
}

- (void)setAudioDataBlock:(void (^)(CMSampleBufferRef, int, int))audioDataBlock {
    @synchronized(_lock) {
        _audioDataBlock = audioDataBlock;
    }
}

- (void)setPlaybackRate:(float)playbackRate {
    _playbackRate = playbackRate;
    if (fabsf(playbackRate - 1.0f) <= 0.000001) {
        UInt32 propValue = 1;
        AudioQueueSetProperty(_audioQueueRef, kAudioQueueProperty_TimePitchBypass, &propValue,
                              sizeof(propValue));
        AudioQueueSetParameter(_audioQueueRef, kAudioQueueParam_PlayRate, 1.0f);
    } else {
        UInt32 propValue = 0;
        AudioQueueSetProperty(_audioQueueRef, kAudioQueueProperty_TimePitchBypass, &propValue,
                              sizeof(propValue));
        AudioQueueSetParameter(_audioQueueRef, kAudioQueueParam_PlayRate, playbackRate);
    }
}

- (void)setPlaybackVolume:(float)playbackVolume {
    _playbackVolume = playbackVolume;
    if (fabsf(_playbackVolume - 1.0f) <= 0.000001) {
        AudioQueueSetParameter(_audioQueueRef, kAudioQueueParam_Volume, 1.f);
    } else {
        AudioQueueSetParameter(_audioQueueRef, kAudioQueueParam_Volume, _playbackVolume);
    }
}

- (double)get_latency_seconds {
    if (_playbackRate > 0 && _playbackRate <= 2) {
        return ((double)(kIJKAudioQueueNumberBuffers)) * _spec.samples /
                   (_spec.freq * _playbackRate) +
               _deviceAoutDelaySec;
    } else {
        return ((double)(kIJKAudioQueueNumberBuffers)) * _spec.samples / _spec.freq +
               _deviceAoutDelaySec;
    }
}

static void IJKSDLAudioQueueOuptutCallback(void* inUserData, AudioQueueRef inAQ,
                                           AudioQueueBufferRef inBuffer) {
    @autoreleasepool {
        /** KingSoft's code begin **/
        OSStatus status;
        /** KingSoft's code end **/
        IJKSDLAudioQueueController* aqController = (__bridge IJKSDLAudioQueueController*)inUserData;

        if (!aqController) {
            // do nothing;
        } else if (aqController->_isPaused || aqController->_isStopped) {
            memset(inBuffer->mAudioData, aqController.spec.silence, inBuffer->mAudioDataByteSize);
        } else {
            (*aqController.spec.callback)(aqController.spec.userdata, inBuffer->mAudioData,
                                          inBuffer->mAudioDataByteSize);
            if (aqController->_isClear) {
                // 避免输出seek的遗留数据而产生噪音
                memset(inBuffer->mAudioData, aqController.spec.silence,
                       inBuffer->mAudioDataByteSize);
                aqController->_isClear = false;
            }
        }

        /** KingSoft's code begin **/
        if (aqController != nil && aqController->_audioDataBlock) {
            do {
                CMBlockBufferRef blockBuffer;
                status = CMBlockBufferCreateWithMemoryBlock(
                    kCFAllocatorDefault, inBuffer->mAudioData, inBuffer->mAudioDataByteSize,
                    kCFAllocatorNull, NULL, 0, inBuffer->mAudioDataByteSize,
                    kCMBlockBufferAssureMemoryNowFlag, &blockBuffer);
                if (status != noErr) {
                    NSLog(@"CMBlockBufferCreateWithMemoryBlock error!\n");
                    break;
                }

                // Timestamp of current sample
                CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
                CFTimeInterval elapsedTime = currentTime - aqController->_startTime;
                CMTime timeStamp = CMTimeMake(elapsedTime * 1000, 1000);

                // Number of samples in the buffer
                // long nSamples = inBuffer->mAudioDataByteSize /
                // mBytesPerFrame;
                CMSampleBufferRef sampleBuffer;
                AudioStreamBasicDescription audioStreamDesc;
                audioStreamDesc.mSampleRate = aqController->_streamDescription.mSampleRate;
                audioStreamDesc.mFormatID = kAudioFormatLinearPCM;
                audioStreamDesc.mFormatFlags =
                    kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
                audioStreamDesc.mBytesPerPacket = aqController->_streamDescription.mBytesPerPacket;
                audioStreamDesc.mFramesPerPacket = 1;
                audioStreamDesc.mBytesPerFrame = aqController->_streamDescription.mBytesPerFrame;
                audioStreamDesc.mChannelsPerFrame =
                    aqController->_streamDescription.mChannelsPerFrame;
                audioStreamDesc.mBitsPerChannel = aqController->_streamDescription.mBitsPerChannel;
                audioStreamDesc.mReserved = 0;

                CMAudioFormatDescriptionRef audioFormatDesc;

                status = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &audioStreamDesc, 0,
                                                        nil, 0, nil, nil, &audioFormatDesc);
                if (status != noErr) {
                    NSLog(@"CMAudioFormatDescriptionCreate error!\n");
                    CFRelease(blockBuffer);
                    break;
                }
                status = CMAudioSampleBufferCreateWithPacketDescriptions(
                    kCFAllocatorDefault, blockBuffer, true, NULL, NULL, audioFormatDesc,
                    aqController->_spec.samples, timeStamp, NULL, &sampleBuffer);
                if (status == noErr) {
                    CMSampleBufferMakeDataReady(sampleBuffer);
                    aqController->_audioDataBlock(sampleBuffer, audioStreamDesc.mSampleRate,
                                                  audioStreamDesc.mChannelsPerFrame);
                    CFRelease(sampleBuffer);
                    CFRelease(blockBuffer);
                    CFRelease(audioFormatDesc);
                }

            } while (0);
        }

        if (aqController && !aqController->_isPaused && !aqController->_isStopped &&
            aqController->_mute) {
            memset(inBuffer->mAudioData, aqController.spec.silence, inBuffer->mAudioDataByteSize);
        }
        /** KingSoft's code end **/
        status = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    }
}

@end
