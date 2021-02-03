//
// Created by MarshallShuai on 2019-11-07.
//

#include <cstring>
#include "aes_decrypt.h"
#include "ac_log.h"


#if (USE_FFMPEG_AES)
extern "C" {
#include <libavutil/mem.h>
}
#endif


HODOR_NAMESPACE_START

AesDecrypt::AesDecrypt(uint8_t* key_128_bit) {
#if (USE_FFMPEG_AES)
    memcpy(key_, key_128_bit, AES_128_BIT_KEY_BYTES);
    ff_aes_ = av_aes_alloc();
    av_aes_init(ff_aes_, key_, 128, 1);
#else
    memcpy(key_, key_128_bit, AES_128_BIT_KEY_BYTES);
    AES_init_ctx(&aes_ctx_, key_);
#endif
}

int64_t AesDecrypt::Decrypt(uint8_t* buf, int64_t buf_len) {

#if (USE_FFMPEG_AES)
    if (buf_len < AES_BLOCK_LEN) {
        return -1;
    }
    av_aes_crypt(ff_aes_, buf, buf, static_cast<int>(buf_len / AES_BLOCK_LEN), nullptr, 1);
    return buf_len / AES_BLOCK_LEN * AES_BLOCK_LEN;
#else
    if (buf_len < AES_BLOCKLEN) {
        LOG_ERROR("[AesDecrypt::Decrypt]input buf_len not enough tou decrypt, buf_len:%lld", buf_len);
        return -1;
    }

    int64_t dec_len = 0;
    while ((buf_len - dec_len) / AES_BLOCKLEN * AES_BLOCKLEN > 0) {
        AES_ECB_decrypt(&aes_ctx_, buf + dec_len);
        dec_len += AES_BLOCKLEN;
    }


    return dec_len > 0 ? dec_len : -1;
#endif
}

AesDecrypt::~AesDecrypt() {
#if (USE_FFMPEG_AES)
    av_freep(&ff_aes_);
#endif
}

HODOR_NAMESPACE_END
