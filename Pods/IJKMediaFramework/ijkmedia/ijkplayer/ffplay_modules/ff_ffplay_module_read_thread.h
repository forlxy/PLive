//
// Created by MarshallShuai on 2019/4/19.
//
#pragma once

/**
 *  直播前后台无缝切换用到的两个线程
 */
int audio_read_thread(void*);
int video_read_thread(void*);

/**
 * 原生的io线程
 */
int read_thread(void*);
