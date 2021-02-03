#include "str_encrypt.h"

static int symmetricTransChar(char* dst, char* src, int len) {
    int i = 0;
    for (i = 0; i < len; ++i) {
        char temp = src[i];
        if (temp >= 0x20 && temp <= 0x7E) {
            dst[i] = 0x9E - temp;
        } else {
            dst[i] = temp;
        }
    }
    return len;
}

int encryptStr(char* dst, char* src, int len) {
    return symmetricTransChar(dst, src, len);
}

int decryptStr(char* dst, char* src, int len) {
    return symmetricTransChar(dst, src, len);
}
