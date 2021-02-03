//
// Created by MarshallShuai on 2019/4/19.
//

#pragma once

#include "ijksdl/ijksdl_stdinc.h"

/**
 * 本函数会在 Android的AudioTrack/gles或者iOS的AudioSessionQueue的audio_thread里调用来获取解码后的samples
 */
void sdl_audio_callback(void* opaque, Uint8* stream, int len);