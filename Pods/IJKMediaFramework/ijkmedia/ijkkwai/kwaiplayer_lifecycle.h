//
// Created by MarshallShuai on 2018/11/12.
// 监控播放器生命周期，主要用来监控播放器泄漏
//
#pragma once

void KwaiPlayerLifeCycle_module_init();
void KwaiPlayerLifeCycle_on_player_created();
void KwaiPlayerLifeCycle_on_player_destroyed();
int KwaiPlayerLifeCycle_get_current_alive_cnt_unsafe();
int KwaiPlayerLifeCycle_get_current_alive_cnt_safe();

