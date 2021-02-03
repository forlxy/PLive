#include "kwai_priv_nal_c.h"
#include "kwai_priv_nal.h"
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

int KwaiPrivNal_init(KwaiPrivNal** kwaiPrivNal) {
    if (kwaiPrivNal) {
        KWAI::PrivNal* priv_nal = new KWAI::PrivNal();
        if (priv_nal && priv_nal->isValid()) {
            *kwaiPrivNal = (KwaiPrivNal*)calloc(1, sizeof(KwaiPrivNal));
            if (*kwaiPrivNal) {
                (*kwaiPrivNal)->context_ = (void*)priv_nal;
                return 1;
            }
        }

        if (priv_nal) {
            delete priv_nal;
            priv_nal = 0;
        }
    }

    return 0;
}

int KwaiPrivNal_init2(KwaiPrivNal** kwaiPrivNal, const char* src, int len, enum AVCodecID codecId, int memCopy) {
    if (kwaiPrivNal && src) {
        KWAI::PrivNal* priv_nal = new KWAI::PrivNal(src, len, codecId, memCopy);
        if (priv_nal && priv_nal->isValid()) {
            *kwaiPrivNal = (KwaiPrivNal*)calloc(1, sizeof(KwaiPrivNal));
            if (*kwaiPrivNal) {
                (*kwaiPrivNal)->context_ = (void*)priv_nal;
                return 1;
            }
        }

        if (priv_nal) {
            delete priv_nal;
            priv_nal = 0;
        }
    }

    return 0;
}

int KwaiPrivNal_free(KwaiPrivNal* kwaiPrivNal) {
    if (kwaiPrivNal) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        if (priv_nal) {
            delete priv_nal;
        }
        kwaiPrivNal->context_ = 0;
        free(kwaiPrivNal);
    }

    return 1;
}

int KwaiPrivNal_isValid(KwaiPrivNal* kwaiPrivNal) {
    if (kwaiPrivNal && kwaiPrivNal->context_) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->isValid();
    }

    return 0;
}

char* KwaiPrivNal_getNalData(KwaiPrivNal* kwaiPrivNal) {
    if (kwaiPrivNal && kwaiPrivNal->context_) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->getNalData();
    }

    return 0;
}

int KwaiPrivNal_getNalLen(KwaiPrivNal* kwaiPrivNal) {
    if (kwaiPrivNal && kwaiPrivNal->context_) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->getNalLen();
    }

    return -1;
}

int KwaiPrivNal_writeElemBuf(KwaiPrivNal* kwaiPrivNal, const char* tag, const char* buf, const int len) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && buf) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->writeElemBuf(tag, buf, len);
    }

    return 0;
}

int KwaiPrivNal_writeElemString(KwaiPrivNal* kwaiPrivNal, const char* tag, const char* str, const int len) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && str) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->writeElemString(tag, std::string(str, len));
    }

    return 0;
}

int KwaiPrivNal_writeElemInt32(KwaiPrivNal* kwaiPrivNal, const char* tag, int32_t value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->writeElemInt32(tag, value);
    }

    return 0;
}

int KwaiPrivNal_writeElemInt64(KwaiPrivNal* kwaiPrivNal, const char* tag, int64_t value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->writeElemInt64(tag, value);
    }

    return 0;
}

int KwaiPrivNal_writeElemFloat(KwaiPrivNal* kwaiPrivNal, const char* tag, float value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->writeElemFloat(tag, value);
    }

    return 0;
}

int KwaiPrivNal_writeElemDouble(KwaiPrivNal* kwaiPrivNal, const char* tag, double value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        return priv_nal->writeElemDouble(tag, value);
    }

    return 0;
}

int KwaiPrivNal_getElemBuf(KwaiPrivNal* kwaiPrivNal, const char* tag, char* buf, int* len) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && buf && len) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        int lenRef = *len;
        if (priv_nal->getElemBuf(tag, buf, lenRef)) {
            *len = lenRef;
            return 1;
        }
    }

    return 0;
}

int KwaiPrivNal_getElemString(KwaiPrivNal* kwaiPrivNal, const char* tag, char* str, int64_t* len) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && str && len) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        std::string value;
        if (priv_nal->getElemString(tag, value)) {
            if (*len >= value.length()) {
                memcpy(str, value.c_str(), value.length());
                *len = value.length();
                return 1;
            }
        }
    }

    return 0;
}

int KwaiPrivNal_getElemInt32(KwaiPrivNal* kwaiPrivNal, const char* tag, int32_t* value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && value) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        int32_t valueRef = *value;
        if (priv_nal->getElemInt32(tag, valueRef)) {
            *value = valueRef;
            return 1;
        }
    }

    return 0;
}

int KwaiPrivNal_getElemInt64(KwaiPrivNal* kwaiPrivNal, const char* tag, int64_t* value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && value) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        int64_t valueRef = *value;
        if (priv_nal->getElemInt64(tag, valueRef)) {
            *value = valueRef;
            return 1;
        }
    }

    return 0;
}

int KwaiPrivNal_getElemFloat(KwaiPrivNal* kwaiPrivNal, const char* tag, float* value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && value) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        float valueRef = *value;
        if (priv_nal->getElemFloat(tag, valueRef)) {
            *value = valueRef;
            return 1;
        }
    }

    return 0;
}

int KwaiPrivNal_getElemDouble(KwaiPrivNal* kwaiPrivNal, const char* tag, double* value) {
    if (kwaiPrivNal && kwaiPrivNal->context_ && tag && value) {
        KWAI::PrivNal* priv_nal = (KWAI::PrivNal*)(kwaiPrivNal->context_);
        double valueRef = *value;
        if (priv_nal->getElemDouble(tag, valueRef)) {
            *value = valueRef;
            return 1;
        }
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
