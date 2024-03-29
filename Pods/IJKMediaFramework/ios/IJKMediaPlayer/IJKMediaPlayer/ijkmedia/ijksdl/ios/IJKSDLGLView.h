/*
 * IJKSDLGLView.h
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

#import <UIKit/UIKit.h>

#define ENABLE_HUD_VIEW 0

#include "ijksdl/ijksdl_vout.h"

@interface IJKSDLGLView : UIView

- (id)initWithFrame:(CGRect)frame sessionId:(int)sessionid;
// 目前返回-1表示渲染错误
- (int)display:(SDL_VoutOverlay*)overlay;

- (UIImage*)snapshot;

- (void)setHudValue:(NSString*)value forKey:(NSString*)key;
- (void)setShouldLockWhileBeingMovedToWindow:(BOOL)shouldLockWhiteBeingMovedToWindow
    __attribute__((deprecated("unused")));

@property(nonatomic, readonly) CGFloat fps;
@property(nonatomic) CGFloat scaleFactor;
@property(nonatomic) BOOL shouldShowHudView;
@property(nonatomic) BOOL shouldDisplayInternal;

@property(nonatomic) int sessionId;  // for debug purpose
/** KingSoft's code begin **/
- (void)setRotateDegress:(int)degress;

@property(nonatomic, copy) void (^videoDataBlock)(CVPixelBufferRef pixelBuffer, int rotation);
/** KingSoft's code end **/

// kwai added for mirror video
- (void)setMirror:(BOOL)mirror;

/**
 * 表示这个view是复用的，默认为NO
 */
@property(nonatomic) BOOL isReusedView;
/**
 * 重置view状态，主要给ReuseView场景使用，app一般用不到
 */
- (int)resetViewStatus;
@end
