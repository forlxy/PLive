//
// Created by MarshallShuai on 2018/11/14.
//

#include <pthread.h>
#include "ac_utils.h"
#include <sys/time.h>

namespace kuaishou {
namespace cache {
void AcUtils::SetThreadName(const std::string& name) {
#ifdef __APPLE__
    pthread_setname_np(name.c_str());
#elif _WIN32
    set_thread_name(name.c_str());
#else
    pthread_setname_np(pthread_self(), name.c_str());
#endif
}

int64_t AcUtils::GetCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

}
}
