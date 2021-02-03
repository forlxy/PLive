//
// Created by MarshallShuai on 2018/11/12.
//

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_alive_cnt;
static bool g_inited = false;

void KwaiPlayerLifeCycle_module_init() {
    pthread_mutex_lock(&g_init_mutex);
    if (!g_inited) {
        g_alive_cnt = 0;
    }
    g_inited = true;
    pthread_mutex_unlock(&g_init_mutex);
}

void KwaiPlayerLifeCycle_on_player_created() {
    pthread_mutex_lock(&g_init_mutex);
    g_alive_cnt++;
    pthread_mutex_unlock(&g_init_mutex);
}

void KwaiPlayerLifeCycle_on_player_destroyed() {
    pthread_mutex_lock(&g_init_mutex);
    g_alive_cnt--;
    pthread_mutex_unlock(&g_init_mutex);
}

/**
 * 这个函数只需要大概准确即可，一般都是高频率的debugInfo来参考，无需完全的线程安全
 */
int KwaiPlayerLifeCycle_get_current_alive_cnt() {
    return g_alive_cnt;
}

int KwaiPlayerLifeCycle_get_current_alive_cnt_unsafe() {
    return g_alive_cnt;
}

int KwaiPlayerLifeCycle_get_current_alive_cnt_safe() {

    int ret;
    pthread_mutex_lock(&g_init_mutex);
    ret = g_alive_cnt--;
    pthread_mutex_unlock(&g_init_mutex);
    return ret;
}
