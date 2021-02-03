/*
 * IJKSDLGLView.m
 *
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
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

#import "IJKSDLGLView.h"
#import "IJKSDLHudViewController.h"
#include "ijksdl/ijksdl_gles2.h"
#include "ijksdl/ijksdl_timer.h"
#include "ijksdl/ios/ijksdl_ios.h"

typedef NS_ENUM(NSInteger, IJKSDLGLViewApplicationState) {
    IJKSDLGLViewApplicationUnknownState = 0,
    IJKSDLGLViewApplicationForegroundState = 1,
    IJKSDLGLViewApplicationBackgroundState = 2
};

@interface IJKSDLGLView ()
@property(atomic, strong) NSRecursiveLock* glActiveLock;
@property(atomic) BOOL glActivePaused;
@end

@implementation IJKSDLGLView {
    EAGLContext* _context;
    GLuint _framebuffer;
    GLuint _renderbuffer;
    GLint _backingWidth;
    GLint _backingHeight;

    int _frameCount;
    BOOL _firstFrameRendered;

    int64_t _lastFrameTime;

    IJK_GLES2_Renderer* _renderer;
    int _rendererGravity;

    BOOL _isRenderBufferInvalidated;

    int _tryLockErrorCount;
    BOOL _didSetupGL;
    BOOL _didStopGL;
    BOOL _didLockedDueToMovedToWindow;
    BOOL _shouldLockWhileBeingMovedToWindow;
    NSMutableArray* _registeredNotifications;

#if ENABLE_HUD_VIEW
    IJKSDLHudViewController* _hudViewController;
#endif
    IJKSDLGLViewApplicationState _applicationState;
    int _degress;
    BOOL _mirror;

    CAEAGLLayer* _eaglLayer;
    int _debugDroppedFrameDueToGLNotReady;
}

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame sessionId:(int)sessionid {
    self = [super initWithFrame:frame];
    if (self) {
        _sessionId = sessionid;
        _tryLockErrorCount = 0;
        _shouldLockWhileBeingMovedToWindow = YES;
        self.glActiveLock = [[NSRecursiveLock alloc] init];
        _registeredNotifications = [[NSMutableArray alloc] init];
        [self registerApplicationObservers];

        _didSetupGL = NO;
        if ([self isApplicationActive] == YES) {
            [self setupGLOnce];
        }

#if ENABLE_HUD_VIEW
        _hudViewController = [[IJKSDLHudViewController alloc] init];
        [self addSubview:_hudViewController.tableView];
#endif

        _degress = 0;
        _mirror = NO;
        _shouldDisplayInternal = YES;
        _debugDroppedFrameDueToGLNotReady = 0;

        // 复用view相关
        _isReusedView = NO;
        _firstFrameRendered = NO;
    }

    return self;
}

- (void)willMoveToWindow:(UIWindow*)newWindow {
    if (!_shouldLockWhileBeingMovedToWindow) {
        [super willMoveToWindow:newWindow];
        return;
    }
    if (newWindow && !_didLockedDueToMovedToWindow) {
        [self lockGLActive];
        _didLockedDueToMovedToWindow = YES;
    }
    [super willMoveToWindow:newWindow];
    ALOGD("[%d][IJKSDLGLView:willMoveToWindow] newWindow:%p, applicationState:%d", _sessionId,
          newWindow, (int)[UIApplication sharedApplication].applicationState);
}

- (void)didMoveToWindow {
    [super didMoveToWindow];
    if (self.window && _didLockedDueToMovedToWindow) {
        [self unlockGLActive];
        _didLockedDueToMovedToWindow = NO;
    }
    ALOGD("[%d][IJKSDLGLView:didMoveToWindow] applicationState:%d", _sessionId,
          (int)[UIApplication sharedApplication].applicationState);
}

- (BOOL)setupEAGLContext:(EAGLContext*)context {
    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_renderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:[self eaglLayer]];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderbuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ALOGE("[%d][IJKSDLGLView:setupEAGLContext] glCheckFramebufferStatus fail: %x\n", _sessionId,
              status);
        return NO;
    }

    GLenum glError = glGetError();
    if (GL_NO_ERROR != glError) {
        ALOGE("[%d][IJKSDLGLView:setupEAGLContext] fail, glGetError return error:%x\n", _sessionId,
              glError);
        return NO;
    }

    return YES;
}

- (CAEAGLLayer*)eaglLayer {
    if (!_eaglLayer) {
        _eaglLayer = (CAEAGLLayer*)self.layer;
    }
    return _eaglLayer;
}

- (BOOL)setupGL {
    if (_didSetupGL) {
        return YES;
    }

    CAEAGLLayer* eaglLayer = self.eaglLayer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = [NSDictionary
        dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:NO],
                                     kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8,
                                     kEAGLDrawablePropertyColorFormat, nil];

    _scaleFactor = [[UIScreen mainScreen] scale];
    if (_scaleFactor < 0.1f) {
        _scaleFactor = 1.0f;
    }

    [eaglLayer setContentsScale:_scaleFactor];

    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (_context == nil) {
        ALOGE("[%d][IJKSDLGLView:setupGL] failed, EAGLContext alloc/init fail \n", _sessionId);
        return NO;
    }

    EAGLContext* prevContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];

    _didSetupGL = NO;
    if ([self setupEAGLContext:_context]) {
        ALOGD("[%d][IJKSDLGLView] setupGL setupEAGLContext success ,isMainThread:%d \n", _sessionId,
              [NSThread isMainThread]);
        _didSetupGL = YES;
    } else {
        ALOGE("[%d][IJKSDLGLView:setupGL] setupEAGLContext fail \n", _sessionId);
    }

    [EAGLContext setCurrentContext:prevContext];
    return _didSetupGL;
}

- (BOOL)setupGLOnce {
    if (_didSetupGL) {
        return YES;
    }

    if (![self tryLockGLActive]) {
        return NO;
    }
    BOOL didSetupGL = [self setupGL];

    [self unlockGLActive];

    return didSetupGL;
}

- (BOOL)isApplicationActive {
    switch (_applicationState) {
        case IJKSDLGLViewApplicationForegroundState:
            return YES;
        case IJKSDLGLViewApplicationBackgroundState:
            return NO;
        default: {
            if ([NSThread isMainThread]) {
                // 如果在主线程，则可以更精准度的参考 applicationState
                UIApplicationState appState = [UIApplication sharedApplication].applicationState;
                switch (appState) {
                    case UIApplicationStateActive:
                        return YES;
                    case UIApplicationStateInactive:
                    case UIApplicationStateBackground:
                    default:
                        return NO;
                }
            } else {
                // 一般在display的子线程才会走到这，在这统一认为是active
                return YES;
            }
        }
    }
}

- (void)dealloc {
    [self lockGLActive];

    _didStopGL = YES;

    EAGLContext* prevContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];

    IJK_GLES2_Renderer_reset(_renderer);
    IJK_GLES2_Renderer_freeP(&_renderer);

    if (_framebuffer) {
        glDeleteFramebuffers(1, &_framebuffer);
        _framebuffer = 0;
    }

    if (_renderbuffer) {
        glDeleteRenderbuffers(1, &_renderbuffer);
        _renderbuffer = 0;
    }

    glFinish();

    [EAGLContext setCurrentContext:prevContext];

    _context = nil;

    [self unregisterApplicationObservers];

    [self unlockGLActive];
}

- (void)setScaleFactor:(CGFloat)scaleFactor {
    _scaleFactor = scaleFactor;
    ALOGD("[%d][IJKSDLGLView:setScaleFactor], to invalidateRenderBuffer", _sessionId);
    [self invalidateRenderBuffer];
}

- (void)layoutSubviews {
    [super layoutSubviews];

    // 客户端外部跳转并使用preDecode模式，未设置视图大小会导致framebuffer初始化失败，当视图更新重新初始化OpenGL
    if (!_didSetupGL) [self setupGLOnce];

    CGRect selfFrame = self.frame;
    CGRect newFrame = selfFrame;

    //    newFrame.size.width   = selfFrame.size.width * 1 / 3;
    //    newFrame.origin.x     = selfFrame.size.width * 2 / 3;
    newFrame.size.width = selfFrame.size.width * 3 / 5;
    newFrame.origin.x = selfFrame.size.width * 2 / 5;

    newFrame.size.height = selfFrame.size.height * 8 / 8;
    newFrame.origin.y += selfFrame.size.height * 0 / 8;

#if ENABLE_HUD_VIEW
    _hudViewController.tableView.frame = newFrame;
#endif
    ALOGD("[%d][IJKSDLGLView:layoutSubviews], to invalidateRenderBuffer", _sessionId);
    [self invalidateRenderBuffer];
}

- (void)setContentMode:(UIViewContentMode)contentMode {
    [super setContentMode:contentMode];

    switch (contentMode) {
        case UIViewContentModeScaleToFill:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE;
            break;
        case UIViewContentModeScaleAspectFit:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE_ASPECT;
            break;
        case UIViewContentModeScaleAspectFill:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE_ASPECT_FILL;
            break;
        default:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE_ASPECT;
            break;
    }
    ALOGD("[%d][IJKSDLGLView:setContentMode], to invalidateRenderBuffer", _sessionId);
    [self invalidateRenderBuffer];
}

- (BOOL)setupRenderer:(SDL_VoutOverlay*)overlay {
    if (overlay == nil) return _renderer != nil;

    if (!IJK_GLES2_Renderer_isValid(_renderer) ||
        !IJK_GLES2_Renderer_isFormat(_renderer, overlay->format)) {
        ALOGD("[%d][IJKSDLGLView:setupRenderer], to re-setup Renderer", _sessionId);
        IJK_GLES2_Renderer_reset(_renderer);
        IJK_GLES2_Renderer_freeP(&_renderer);

        _renderer = IJK_GLES2_Renderer_create(overlay);
        if (!IJK_GLES2_Renderer_isValid(_renderer)) return NO;

        if (!IJK_GLES2_Renderer_use(_renderer)) return NO;

        IJK_GLES2_Renderer_setRotateDegress(_renderer, _degress);
        IJK_GLES2_Renderer_setMirror(_renderer, _mirror);
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, _backingWidth, _backingHeight);
    }

    return YES;
}

// 滑滑版右滑的时候invalidateRenderBuffer回调用频繁，这块不需要每次都亲自validateRenderBuffer,在display内部已经有validate操作
- (void)invalidateRenderBuffer {
    _isRenderBufferInvalidated = YES;

    // 原有逻辑
    //    NSLog(@"[IJKSDLGLView] invalidateRenderBuffer lockGLActive");
    //    [self lockGLActive];
    //
    //    _isRenderBufferInvalidated = YES;
    //
    //    if ([[NSThread currentThread] isMainThread]) {
    //        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
    //            if (_isRenderBufferInvalidated) {
    //                [self display:nil];
    //            }
    //        });
    //    } else {
    //        [self display:nil];
    //    }
    //
    //    NSLog(@"[IJKSDLGLView] invalidateRenderBuffer unlockGLActive");
    //    [self unlockGLActive];
}

// overlay = nil 表示只为了setup openGL相关的组件
- (int)display:(nullable SDL_VoutOverlay*)overlay {
    if ([self isReneredByOutsiders:overlay]) {
        [self onFrameRendered];
        return 0;
    } else {
        if (_didSetupGL == NO) {
            // overlay 表示是只为了初始化，不准备渲染帧
            if (overlay) {
                _debugDroppedFrameDueToGLNotReady++;
                ALOGW("[%d][IJKSDLGLView:display] GL not setup, drop frame, "
                      "debugDroppedFrameDueToGLNotReady:%d",
                      _sessionId, _debugDroppedFrameDueToGLNotReady);
            }
            return -1;
        }

        if ([self isApplicationActive] == NO) {
            return -1;
        }

        if (![self tryLockGLActive]) {
            if (0 == (_tryLockErrorCount % 10)) {
                ALOGW("[%d][IJKSDLGLView:display] tryLockGLActive fail: %d\n", _sessionId,
                      _tryLockErrorCount);
            }
            _tryLockErrorCount++;
            // todo add qos drop frame ,shuai
            return -1;
        }
        _tryLockErrorCount = 0;

        int ret = 0;
        if (_context && !_didStopGL) {
            EAGLContext* prevContext = [EAGLContext currentContext];
            [EAGLContext setCurrentContext:_context];
            ret = [self displayInternal:overlay];
            [EAGLContext setCurrentContext:prevContext];
        }

        [self unlockGLActive];

        return ret;
    }
}

- (void)onFrameRendered {
    int64_t current = (int64_t)SDL_GetTickHR();
    int64_t delta = (current > _lastFrameTime) ? current - _lastFrameTime : 0;
    if (delta <= 0) {
        _lastFrameTime = current;
    } else if (delta >= 1000) {
        _fps = ((CGFloat)_frameCount) * 1000 / delta;
        _frameCount = 0;
        _lastFrameTime = current;
    } else {
        _frameCount++;
    }
}

- (int)displayInternal:(nullable SDL_VoutOverlay*)overlay {
    if (![self setupRenderer:overlay]) {
        if (!overlay && !_renderer) {
            ALOGW("[%d][IJKSDLGLView:displayInternal]: Renderer not ready overlay:%p, "
                  "_renderer:%p, return \n",
                  _sessionId, overlay, _renderer);
        } else {
            ALOGE("[%d][IJKSDLGLView:displayInternal]: setupDisplay failed, return \n", _sessionId);
        }
        return -1;
    }

    if (_isRenderBufferInvalidated) {
        ALOGD("[%d][IJKSDLGLView:displayInternal] _isRenderBufferInvalidated=YES, to validate\n",
              _sessionId);
        _isRenderBufferInvalidated = NO;

        // 这块逻辑会和和xcode的Hierarychy Debug有冲突，会导致View发生死锁冲突
        // 但是不能注释掉，不然显示会不全面，后续只能通过 后续计划吧IJKSDLGLView换成GLKView实现
        // 来解决
        glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);
        [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:[self eaglLayer]];
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_backingWidth);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_backingHeight);
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, _backingWidth, _backingHeight);
        // MIDD-2306 修复MV Master视频导出后，视频播放右侧会有一像素绿边的bug
        _backingWidth += _backingWidth % 2;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glViewport(0, 0, _backingWidth, _backingHeight);

    if (!IJK_GLES2_Renderer_renderOverlay(_renderer, overlay)) {
        ALOGE("[%d][IJKSDLGLView:displayInternal] IJK_GLES2_Renderer_renderOverlay failed\n",
              _sessionId);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);
    [_context presentRenderbuffer:GL_RENDERBUFFER];

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    [self onFrameRendered];

    if (_isReusedView && !_firstFrameRendered) {
        ALOGD("[%d][IJKSDLGLView displayInternal] _isReusedView = true, to show view in first "
              "display",
              _sessionId);
        dispatch_sync(dispatch_get_main_queue(), ^{
            self.hidden = NO;
            _firstFrameRendered = YES;
        });
    }

    return 0;
}

/** KingSoft's code begin **/
- (void)setRotateDegress:(int)degress {
    if (IJK_GLES2_Renderer_isValid(_renderer))
        IJK_GLES2_Renderer_setRotateDegress(_renderer, degress);
    _degress = degress;
    return;
}
/** KingSoft's code end **/

- (void)setMirror:(BOOL)mirror {
    if (_mirror == mirror) return;

    if (IJK_GLES2_Renderer_isValid(_renderer)) {
        IJK_GLES2_Renderer_setMirror(_renderer, mirror);
    }
    _mirror = mirror;
    return;
}

- (int)resetViewStatus {
    self.hidden = YES;
    _firstFrameRendered = NO;
    return 0;
}

#pragma mark AppDelegate

- (void)lockGLActive {
    [self.glActiveLock lock];
}

- (void)unlockGLActive {
    [self.glActiveLock unlock];
}

- (BOOL)tryLockGLActive {
    if (![self.glActiveLock tryLock]) {
        return NO;
    }

    /*-
     if ([UIApplication sharedApplication].applicationState !=
     UIApplicationStateActive && [UIApplication
     sharedApplication].applicationState != UIApplicationStateInactive) {
     [self.appLock unlock];
     return NO;
     }
     */

    if (self.glActivePaused) {
        [self.glActiveLock unlock];
        return NO;
    }

    return YES;
}

- (void)toggleGLPaused:(BOOL)paused {
    [self lockGLActive];
    if (!self.glActivePaused && paused) {
        if (_context != nil) {
            EAGLContext* prevContext = [EAGLContext currentContext];
            [EAGLContext setCurrentContext:_context];
            glFinish();
            [EAGLContext setCurrentContext:prevContext];
        }
    }
    self.glActivePaused = paused;
    [self unlockGLActive];
}

- (void)registerApplicationObservers {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillEnterForeground)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    [_registeredNotifications addObject:UIApplicationWillEnterForegroundNotification];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationDidBecomeActive)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    [_registeredNotifications addObject:UIApplicationDidBecomeActiveNotification];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillResignActive)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    [_registeredNotifications addObject:UIApplicationWillResignActiveNotification];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationDidEnterBackground)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    [_registeredNotifications addObject:UIApplicationDidEnterBackgroundNotification];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate)
                                                 name:UIApplicationWillTerminateNotification
                                               object:nil];
    [_registeredNotifications addObject:UIApplicationWillTerminateNotification];
}

- (void)unregisterApplicationObservers {
    for (NSString* name in _registeredNotifications) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:name object:nil];
    }
}

- (void)applicationWillEnterForeground {
    ALOGI("[%d][IJKSDLGLView:applicationWillEnterForeground]: applicationState:%d", _sessionId,
          (int)[UIApplication sharedApplication].applicationState);
    [self toggleGLPaused:NO];
}

- (void)applicationDidBecomeActive {
    ALOGI("[%d][IJKSDLGLView:applicationDidBecomeActive]: applicationState:%d", _sessionId,
          (int)[UIApplication sharedApplication].applicationState);
    [self setupGLOnce];
    _applicationState = IJKSDLGLViewApplicationForegroundState;
    [self toggleGLPaused:NO];
}

- (void)applicationWillResignActive {
    ALOGI("[%d][IJKSDLGLView:applicationWillResignActive]: applicationState:%d", _sessionId,
          (int)[UIApplication sharedApplication].applicationState);
    _applicationState = IJKSDLGLViewApplicationBackgroundState;
    [self toggleGLPaused:YES];
}

- (void)applicationDidEnterBackground {
    ALOGI("[%d][IJKSDLGLView:applicationDidEnterBackground]: applicationState:%d", _sessionId,
          (int)[UIApplication sharedApplication].applicationState);
    _applicationState = IJKSDLGLViewApplicationBackgroundState;
    [self toggleGLPaused:YES];
}

- (void)applicationWillTerminate {
    ALOGI("[%d][IJKSDLGLView:applicationWillTerminate]: applicationState:%d", _sessionId,
          (int)[UIApplication sharedApplication].applicationState);
    _applicationState = IJKSDLGLViewApplicationBackgroundState;
    [self toggleGLPaused:YES];
}

#pragma mark snapshot

- (UIImage*)snapshot {
    [self lockGLActive];

    UIImage* image = [self snapshotInternal];

    [self unlockGLActive];

    return image;
}

- (UIImage*)snapshotInternal {
    if (isIOS7OrLater()) {
        return [self snapshotInternalOnIOS7AndLater];
    } else {
        return [self snapshotInternalOnIOS6AndBefore];
    }
}

- (UIImage*)snapshotInternalOnIOS7AndLater {
    if (CGSizeEqualToSize(self.bounds.size, CGSizeZero)) {
        return nil;
    }
    UIGraphicsBeginImageContextWithOptions(self.bounds.size, NO, 0.0);
    // Render our snapshot into the image context
    [self drawViewHierarchyInRect:self.bounds afterScreenUpdates:NO];

    // Grab the image from the context
    UIImage* complexViewImage = UIGraphicsGetImageFromCurrentImageContext();
    // Finish using the context
    UIGraphicsEndImageContext();

    return complexViewImage;
}

- (UIImage*)snapshotInternalOnIOS6AndBefore {
    EAGLContext* prevContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];

    GLint backingWidth, backingHeight;

    // Bind the color renderbuffer used to render the OpenGL ES view
    // If your application only creates a single color renderbuffer which is
    // already bound at this point, this call is redundant, but it is needed if
    // you're dealing with multiple renderbuffers. Note, replace
    // "viewRenderbuffer" with the actual name of the renderbuffer object
    // defined in your class.
    glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);

    // Get the size of the backing CAEAGLLayer
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    NSInteger x = 0, y = 0, width = backingWidth, height = backingHeight;
    NSInteger dataLength = width * height * 4;
    GLubyte* data = (GLubyte*)malloc(dataLength * sizeof(GLubyte));

    // Read pixel data from the framebuffer
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels((int)x, (int)y, (int)width, (int)height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Create a CGImage with the pixel data
    // If your OpenGL ES content is opaque, use kCGImageAlphaNoneSkipLast to
    // ignore the alpha channel otherwise, use kCGImageAlphaPremultipliedLast
    CGDataProviderRef ref = CGDataProviderCreateWithData(NULL, data, dataLength, NULL);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGImageRef iref = CGImageCreate(width, height, 8, 32, width * 4, colorspace,
                                    kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast, ref,
                                    NULL, true, kCGRenderingIntentDefault);

    [EAGLContext setCurrentContext:prevContext];

    // OpenGL ES measures data in PIXELS
    // Create a graphics context with the target size measured in POINTS
    UIGraphicsBeginImageContext(CGSizeMake(width, height));

    CGContextRef cgcontext = UIGraphicsGetCurrentContext();
    // UIKit coordinate system is upside down to GL/Quartz coordinate system
    // Flip the CGImage by rendering it to the flipped bitmap context
    // The size of the destination area is measured in POINTS
    CGContextSetBlendMode(cgcontext, kCGBlendModeCopy);
    CGContextDrawImage(cgcontext, CGRectMake(0.0, 0.0, width, height), iref);

    // Retrieve the UIImage from the current context
    UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    // Clean up
    free(data);
    CFRelease(ref);
    CFRelease(colorspace);
    CGImageRelease(iref);

    return image;
}

#pragma mark IJKFFHudController
- (void)setHudValue:(NSString*)value forKey:(NSString*)key {
#if ENABLE_HUD_VIEW
    if ([[NSThread currentThread] isMainThread]) {
        [_hudViewController setHudValue:value forKey:key];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self setHudValue:value forKey:key];
        });
    }
#endif
}

- (void)setShouldLockWhileBeingMovedToWindow:(BOOL)shouldLockWhileBeingMovedToWindow {
    _shouldLockWhileBeingMovedToWindow = shouldLockWhileBeingMovedToWindow;
}

- (void)setShouldShowHudView:(BOOL)shouldShowHudView {
#if ENABLE_HUD_VIEW
    _hudViewController.tableView.hidden = !shouldShowHudView;
#endif
}

- (BOOL)shouldShowHudView {
#if ENABLE_HUD_VIEW
    return !_hudViewController.tableView.hidden;
#else
    return NO;
#endif
}

#pragma mark should display internal
- (void)setShouldDisplayInternal:(BOOL)shouldDisplayInternal {
    _shouldDisplayInternal = shouldDisplayInternal;
}

- (BOOL)createPixelBuffer:(SDL_VoutOverlay*)overlay buf:(CVPixelBufferRef*)buf {
    if (!overlay || !overlay->pixels[0]) return FALSE;

    OSType format_type = overlay->color_range == AVCOL_RANGE_JPEG
                             ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                             : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

    NSDictionary* options = [NSDictionary
        dictionaryWithObjectsAndKeys:@(overlay->w), kCVPixelBufferWidthKey, @(overlay->h),
                                     kCVPixelBufferHeightKey, @(overlay->pitches[0]),
                                     kCVPixelBufferBytesPerRowAlignmentKey, @(format_type),
                                     kCVPixelBufferPixelFormatTypeKey,
                                     [NSNumber numberWithBool:YES],
                                     kCVPixelBufferOpenGLESCompatibilityKey,
                                     [NSDictionary dictionary],
                                     kCVPixelBufferIOSurfacePropertiesKey, nil];

    if (overlay->pitches[1] != overlay->pitches[2]) {
        return FALSE;
    }

    CVReturn ret = CVPixelBufferCreate(kCFAllocatorDefault, overlay->w, overlay->h,
                                       kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                                       (__bridge CFDictionaryRef)(options), buf);
    if (ret != kCVReturnSuccess) {
        NSLog(@"CVPixelBufferCreate Failed");
        return FALSE;
    }

    CVPixelBufferLockBaseAddress(*buf, 0);

    size_t bytePerRowY = CVPixelBufferGetBytesPerRowOfPlane(*buf, 0);
    size_t bytesPerRowUV = CVPixelBufferGetBytesPerRowOfPlane(*buf, 1);

    uint8_t* base = CVPixelBufferGetBaseAddressOfPlane(*buf, 0);
    for (size_t i = 0; i < overlay->h; i++) {
        memcpy(base + i * bytePerRowY, overlay->pixels[0] + i * overlay->pitches[0],
               overlay->pitches[0]);
    }

    base = CVPixelBufferGetBaseAddressOfPlane(*buf, 1);
    memset(base, 0, bytesPerRowUV * overlay->h / 2);

    for (size_t i = 0; i < overlay->h / 2; i++) {
        for (size_t j = 0; j < overlay->pitches[1]; j++) {
            base[2 * j + i * bytesPerRowUV] = overlay->pixels[1][j + i * overlay->pitches[1]];
            base[2 * j + 1 + i * bytesPerRowUV] = overlay->pixels[2][j + i * overlay->pitches[1]];
        }
    }
    CVPixelBufferUnlockBaseAddress(*buf, 0);
    return TRUE;
}

- (BOOL)isReneredByOutsiders:(SDL_VoutOverlay*)overlay {
    if (!_shouldDisplayInternal && overlay != nil && _videoDataBlock) {
        CVPixelBufferRef pixel_buffer = nil;
        if (overlay->format == SDL_FCC__VTB) {
            extern CVPixelBufferRef SDL_VoutOverlayVideoToolBox_GetCVPixelBufferRef(
                SDL_VoutOverlay * overlay);
            pixel_buffer = SDL_VoutOverlayVideoToolBox_GetCVPixelBufferRef(overlay);
            if (pixel_buffer) {
                _videoDataBlock(pixel_buffer, overlay->rotation);
            }
        } else if (overlay->format == SDL_FCC_I420) {
            // it seems that the upper app coulnn't use this data encapsulation
            /*
             size_t planeWidth[3]        = { overlay->w, overlay->w/2,
             overlay->w/2 }; size_t planeHeight[3]       = { overlay->h,
             overlay->h/2, overlay->h/2 }; size_t planeBytesPerRow[3]  = {
             overlay->pitches[0], overlay->pitches[1], overlay->pitches[2] };
             void *planeBaseAddress[3]   = { overlay->pixels[0],
             overlay->pixels[1],  overlay->pixels[2] }; CVReturn ret =
             CVPixelBufferCreateWithPlanarBytes(kCFAllocatorDefault, overlay->w,
             overlay->h,
             kCVPixelFormatType_420YpCbCr8Planar, NULL, 0, 3,
             planeBaseAddress,
             planeWidth, planeHeight, planeBytesPerRow,
             releaseCallback, NULL, NULL, &pixel_buffer);
             if (ret != kCVReturnSuccess){
             NSLog(@"CVPixelBufferCreateWithBytes error:%d", ret);
             break;
             }
             */
            if ([self createPixelBuffer:overlay buf:&pixel_buffer]) {
                if (pixel_buffer) {
                    _videoDataBlock(pixel_buffer, overlay->rotation);
                    CFRelease(pixel_buffer);
                }
            }
        }
        return YES;
    } else {
        return NO;
    }
}
@end
