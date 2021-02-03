//
// Created by MarshallShuai on 2019-11-07.
//

#pragma once

#include <stdint.h>
#include "macro_util.h"

#define PROFILE_AES_DEC 0
// 性能结果：200K视频，开源实现的首屏要增加1.5秒左右，ffmpeg只需要增加不到30ms
#define USE_FFMPEG_AES 1


#if (USE_FFMPEG_AES)
extern "C" {
#include <libavutil/aes.h>
}
#else
#include "utils/aes.hpp"
#define CBC 1
#define CTR 1
#define ECB 1
#endif

HODOR_NAMESPACE_START

#define AES_BLOCK_LEN 16

class AesDecrypt {
  public:
    AesDecrypt(uint8_t* key_128_bit);
    ~AesDecrypt();
    /**
     *
     * @param buf 密文
     * @param buf_len 密文长度
     * @return 实际解码的长度
     */
    int64_t Decrypt(uint8_t* buf, int64_t buf_len);

  private:
#define AES_128_BIT_KEY_BYTES 16
    uint8_t key_[AES_128_BIT_KEY_BYTES] {};

#if (USE_FFMPEG_AES)
    AVAES* ff_aes_;
#else
    AES_ctx aes_ctx_ {};
    uint8_t padding_buf_[AES_BLOCKLEN] {};
#endif
};

HODOR_NAMESPACE_END
