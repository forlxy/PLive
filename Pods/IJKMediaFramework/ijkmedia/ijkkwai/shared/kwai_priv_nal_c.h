#ifndef __KWAI_PRIV_NAL_C_H__
#define __KWAI_PRIV_NAL_C_H__

#include <stdint.h>
#include <libavcodec/avcodec.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KwaiPrivNal_t {
    void* context_;
} KwaiPrivNal;

// return 0: fail; non-zero: ok
int KwaiPrivNal_init(KwaiPrivNal** kwaiPrivNal);
int KwaiPrivNal_init2(KwaiPrivNal** kwaiPrivNal, const char* src, int len, enum AVCodecID codecId, int memCopy);
int KwaiPrivNal_free(KwaiPrivNal* kwaiPrivNal);

// return 0: false; non-zero: true
int KwaiPrivNal_isValid(KwaiPrivNal* kwaiPrivNal);

// return 0: fail; non-zero: address of data
char* KwaiPrivNal_getNalData(KwaiPrivNal* kwaiPrivNal);

// return <0: fail; otherwise: length
int KwaiPrivNal_getNalLen(KwaiPrivNal* kwaiPrivNal);

// return 0: fail; non-zero: ok
int KwaiPrivNal_writeElemBuf(KwaiPrivNal* kwaiPrivNal, const char* tag, const char* buf, const int len);
int KwaiPrivNal_writeElemString(KwaiPrivNal* kwaiPrivNal, const char* tag, const char* str, const int len);
int KwaiPrivNal_writeElemInt32(KwaiPrivNal* kwaiPrivNal, const char* tag, int32_t value);
int KwaiPrivNal_writeElemInt64(KwaiPrivNal* kwaiPrivNal, const char* tag, int64_t value);
int KwaiPrivNal_writeElemFloat(KwaiPrivNal* kwaiPrivNal, const char* tag, float value);
int KwaiPrivNal_writeElemDouble(KwaiPrivNal* kwaiPrivNal, const char* tag, double value);
int KwaiPrivNal_getElemBuf(KwaiPrivNal* kwaiPrivNal, const char* tag, char* buf, int* len);
int KwaiPrivNal_getElemString(KwaiPrivNal* kwaiPrivNal, const char* tag, char* str, int64_t* len);
int KwaiPrivNal_getElemInt32(KwaiPrivNal* kwaiPrivNal, const char* tag, int32_t* value);
int KwaiPrivNal_getElemInt64(KwaiPrivNal* kwaiPrivNal, const char* tag, int64_t* value);
int KwaiPrivNal_getElemFloat(KwaiPrivNal* kwaiPrivNal, const char* tag, float* value);
int KwaiPrivNal_getElemDouble(KwaiPrivNal* kwaiPrivNal, const char* tag, double* value);

#ifdef __cplusplus
}
#endif

#endif // __KWAI_PRIV_NAL_C_H__
