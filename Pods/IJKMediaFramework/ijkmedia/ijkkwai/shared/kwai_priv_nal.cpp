#include "kwai_priv_nal.h"
#include <cstdlib>
#include <cassert>
#include <arpa/inet.h>

namespace KWAI {

PrivNal::PrivNal() :
    is_valid_(false),
    nal_buf_(0),
    nal_len_(0),
    is_buf_alloc_(true),
    been_parsed_(false),
    unescape_buf_(0),
    unescape_len_(0),
    elems_info_(),
    read_net_endian_(false) {
    nal_buf_ = (char*)calloc(1, PRIV_NAL_MAX_LEN);

    if (nal_buf_) {
        nal_buf_[0] = nal_buf_[1] = nal_buf_[2] = 0x00;
        nal_buf_[3] = 0x01; // start code: 0, 0, 0, 1
        int index = 4;

        uint32_t magic = PRIV_NAL_MAGIC_H264;
        memcpy(&nal_buf_[index], &magic, sizeof(magic));
        index += sizeof(magic);

        nal_len_ = PRIV_NAL_HEADER_LEN; // self-included, will be overwrite
        memcpy(&nal_buf_[index], &nal_len_, sizeof(nal_len_));
        index += sizeof(nal_len_);

        assert(PRIV_NAL_HEADER_LEN == index);

        is_valid_ = true;
    }
}

PrivNal::PrivNal(const char* buf, const int len, enum AVCodecID codecId, bool copyMem /* = false */) :
    is_valid_(false),
    nal_buf_(0),
    nal_len_(0),
    is_buf_alloc_(copyMem),
    been_parsed_(false),
    unescape_buf_(0),
    unescape_len_(0),
    elems_info_(),
    read_net_endian_(false) {
    if (buf && len >= PRIV_NAL_HEADER_LEN) {
        uint32_t priv_nal_magic = PRIV_NAL_MAGIC_H264;
        if (codecId == AV_CODEC_ID_H264) {
            priv_nal_magic = PRIV_NAL_MAGIC_H264;
        } else if (codecId == AV_CODEC_ID_HEVC) {
            priv_nal_magic = PRIV_NAL_MAGIC_HEVC;
        }
        uint32_t magic = 0;
        memcpy(&magic, buf + 4, sizeof(magic));
        read_net_endian_ = (priv_nal_magic == ntohl(magic));
        bool got_magic = (priv_nal_magic == magic) || read_net_endian_;

        memcpy(&nal_len_, buf + 8, sizeof(nal_len_));
        if (read_net_endian_) {
            nal_len_ = ntohs(nal_len_);
        }

        if (got_magic && nal_len_ <= len) {
            if (is_buf_alloc_) {
                nal_buf_ = (char*)calloc(1, PRIV_NAL_MAX_LEN);
                if (nal_buf_) {
                    memcpy(nal_buf_, buf, nal_len_);
                    is_valid_ = true;
                }
            } else {
                nal_buf_ = const_cast<char*>(buf);
                is_valid_ = true;
            }
        }
    }
}

PrivNal::~PrivNal() {
    if (is_buf_alloc_ && nal_buf_) {
        free(nal_buf_);
        nal_buf_ = 0;
    }

    if (unescape_buf_) {
        free(unescape_buf_);
        unescape_buf_ = 0;
    }
}

bool PrivNal::writeElemBuf(const std::string& tag, const char* buf, const int len) {
    return writeElem(tag, TYPE_BUF, buf, len);
}

bool PrivNal::writeElemString(const std::string& tag, const std::string& value) {
    return writeElem(tag, TYPE_STRING, value.c_str(), value.length());
}

bool PrivNal::writeElemInt32(const std::string& tag, int32_t value) {
    int32_t value_n = htonl(value);
    return writeElem(tag, TYPE_INT32, (const char*)(&value_n), sizeof(value_n));
}

bool PrivNal::writeElemInt64(const std::string& tag, int64_t value) {
    int64_t value_n = htonl((int32_t)(value & 0xFFFFFFFF)) | ((int64_t)(htonl((int32_t)((value >> 32) & 0xFFFFFFFF))) << 32);
    return writeElem(tag, TYPE_INT64, (const char*)(&value_n), sizeof(value_n));
}

bool PrivNal::writeElemFloat(const std::string& tag, float value) {
    FourBytes bytes = {0};
    bytes.value_float = value;
    int32_t value_n = htonl(bytes.value_int32);
    return writeElem(tag, TYPE_FLOAT, (const char*)(&value_n), sizeof(value_n));
}

bool PrivNal::writeElemDouble(const std::string& tag, double value) {
    EightBytes bytes = {0};
    bytes.value_double = value;
    int64_t value_n = htonl((int32_t)(bytes.value_int64 & 0xFFFFFFFF)) | ((int64_t)(htonl((int32_t)((bytes.value_int64 >> 32) & 0xFFFFFFFF))) << 32);
    return writeElem(tag, TYPE_DOUBLE, (const char*)(&value_n), sizeof(value_n));
}

bool PrivNal::getElemBuf(const std::string& tag, char* buf, int& len) {
    int index = getElemIndex(tag, TYPE_BUF);
    if (index < 0) {
        return false;
    }

    uint16_t value_len;
    memcpy(&value_len, unescape_buf_ + index, sizeof(value_len));
    if (read_net_endian_) {
        value_len = ntohs(value_len);
    }

    if ((int)value_len > len) {
        return false;
    }

    len = (int)value_len;
    memcpy(buf, unescape_buf_ + index + sizeof(uint16_t), len);

    return true;
}

bool PrivNal::getElemString(const std::string& tag, std::string& value) {
    int index = getElemIndex(tag, TYPE_STRING);
    if (index < 0) {
        return false;
    }

    uint16_t value_len;
    memcpy(&value_len, unescape_buf_ + index, sizeof(value_len));
    if (read_net_endian_) {
        value_len = htons(value_len);
    }

    value = std::string(unescape_buf_ + index + sizeof(value_len), value_len);

    return true;
}

bool PrivNal::getElemInt32(const std::string& tag, int32_t& value) {
    int index = getElemIndex(tag, TYPE_INT32);
    if (index < 0) {
        return false;
    }

    int32_t value_n;
    memcpy(&value_n, unescape_buf_ + index, sizeof(value_n));
    value = ntohl(value_n);

    return true;
}

bool PrivNal::getElemInt64(const std::string& tag, int64_t& value) {
    int index = getElemIndex(tag, TYPE_INT64);
    if (index < 0) {
        return false;
    }

    int64_t value_n;
    memcpy(&value_n, unescape_buf_ + index, sizeof(value_n));
    value = ntohl((int32_t)(value_n & 0xFFFFFFFF)) | ((int64_t)(ntohl((int32_t)((value_n >> 32) & 0xFFFFFFFF))) << 32);

    return true;
}

bool PrivNal::getElemFloat(const std::string& tag, float& value) {
    int index = getElemIndex(tag, TYPE_FLOAT);
    if (index < 0) {
        return false;
    }

    int32_t value_n;
    memcpy(&value_n, unescape_buf_ + index, sizeof(value_n));
    FourBytes bytes = {0};
    bytes.value_int32 = ntohl(value_n);
    value = bytes.value_float;

    return true;
}

bool PrivNal::getElemDouble(const std::string& tag, double& value) {
    int index = getElemIndex(tag, TYPE_DOUBLE);
    if (index < 0) {
        return false;
    }

    int64_t value_n;
    memcpy(&value_n, unescape_buf_ + index, sizeof(value_n));
    EightBytes bytes = {0};
    bytes.value_int64 = ntohl((int32_t)(value_n & 0xFFFFFFFF)) | ((int64_t)(ntohl((int32_t)((value_n >> 32) & 0xFFFFFFFF))) << 32);
    value = bytes.value_double;

    return true;
}

bool PrivNal::writeElem(const std::string& tag, KWAI::PrivNal::PrivNalElemType type, const char* value, int len) {
    if (!is_valid_ || !is_buf_alloc_ || !value) {
        return false;
    }

    // typeValue(uint16) + elemLen(uint16) + tagLen(uint16) + tag('tagLen' bytes) + [ valueLen(uint16) if BUF||STRING ] + value('valueLen' bytes)
    int total_len = 3 * sizeof(uint16_t) + tag.length() + ((TYPE_BUF == type || TYPE_STRING == type) ? sizeof(uint16_t) : 0) + len;
    if (nal_len_ + total_len > PRIV_NAL_MAX_LEN) {
        return false;
    }

    // write elem before doing escape
    {
        int temp_len = nal_len_;
        uint16_t type_value = (uint16_t)type;
        memcpy(nal_buf_ + temp_len, &type_value, sizeof(type_value));
        temp_len += sizeof(type_value);

        uint16_t elem_len = (uint16_t)total_len;
        memcpy(nal_buf_ + temp_len, &elem_len, sizeof(elem_len));
        temp_len += sizeof(elem_len);

        uint16_t tag_len = (uint16_t)tag.length();
        memcpy(nal_buf_ + temp_len, &tag_len, sizeof(tag_len));
        temp_len += sizeof(tag_len);
        memcpy(nal_buf_ + temp_len, tag.c_str(), tag_len);
        temp_len += tag_len;

        if (TYPE_BUF == type || TYPE_STRING == type) {
            uint16_t value_len = (uint16_t)len;
            memcpy(nal_buf_ + temp_len, &value_len, sizeof(value_len));
            temp_len += sizeof(value_len);
        }

        memcpy(nal_buf_ + temp_len, value, len);
        temp_len += len;

        assert(temp_len - nal_len_ == total_len);
    }

    // escape: (0x000000/0x000001/0x000002/0x000003) --> (0x00000300/0x00000301/0x00000302/0x00000303)
    {
        char data_temp[PRIV_NAL_MAX_LEN * 2] = {0};
        int data_temp_len = 0;
        char last_two_bytes[2];
        last_two_bytes[0] = nal_buf_[nal_len_ - 2];
        last_two_bytes[1] = nal_buf_[nal_len_ - 1];
        int i = nal_len_;
        while (i < nal_len_ + total_len) {
            char cur_byte = nal_buf_[i];
            if (0x00 == last_two_bytes[0] && 0x00 == last_two_bytes[1] && 0x00 == (0xFC & cur_byte)) {
                data_temp[data_temp_len++] = 0x03;
                last_two_bytes[0] = 0x03;
            } else {
                last_two_bytes[0] = last_two_bytes[1];
            }
            last_two_bytes[1] = cur_byte;
            data_temp[data_temp_len++] = cur_byte;
            ++i;
        }

        if (nal_len_ + data_temp_len > PRIV_NAL_MAX_LEN) {
            return false;
        }

        memcpy(nal_buf_ + nal_len_, data_temp, data_temp_len);
        nal_len_ += data_temp_len;
    }

    memcpy(&nal_buf_[8], &nal_len_, sizeof(nal_len_)); // update 'nal_len' in header

    been_parsed_ = false; // need re-parse after new elem written

    return true;
}

int PrivNal::getElemIndex(const std::string& tag, KWAI::PrivNal::PrivNalElemType type) {
    if (!been_parsed_) {
        been_parsed_ = parse();
    }

    if (been_parsed_) {
        std::map<std::string, std::pair<uint16_t, int> >::iterator it = elems_info_.find(tag);
        if (it != elems_info_.end()) {
            uint16_t type_value = it->second.first;
            if (type_value == (uint16_t)type) {
                return it->second.second;
            }
        }
    }

    return -1;
}

bool PrivNal::parse() {
    been_parsed_ = false;

    if (!is_valid_) {
        return false;
    }

    if (!unescape_buf_) {
        unescape_buf_ = (char*)calloc(1, PRIV_NAL_MAX_LEN);
        if (!unescape_buf_) {
            return false;
        }
    }

    // unescape: 0x000003 --> 0x0000
    {
        memcpy(unescape_buf_, nal_buf_, PRIV_NAL_HEADER_LEN);
        unescape_len_ = PRIV_NAL_HEADER_LEN;

        char last_two_bytes[2];
        last_two_bytes[0] = nal_buf_[unescape_len_ - 2];
        last_two_bytes[1] = nal_buf_[unescape_len_ - 1];

        int i = PRIV_NAL_HEADER_LEN;
        while (i < nal_len_) {
            char cur_byte = nal_buf_[i];
            if (0x00 == last_two_bytes[0] && 0x00 == last_two_bytes[1] && 0x03 == cur_byte) {
                // discard 0x03
            } else {
                unescape_buf_[unescape_len_++] = cur_byte;
            }
            last_two_bytes[0] = last_two_bytes[1];
            last_two_bytes[1] = cur_byte;
            ++i;
        }
    }

    elems_info_.clear();
    int index = PRIV_NAL_HEADER_LEN;
    while (index < unescape_len_) {
        int tempIndex = index;
        uint16_t type_value = *((uint16_t*)(unescape_buf_ + tempIndex));
        if (read_net_endian_) {
            type_value = ntohs(type_value);
        }
        tempIndex += sizeof(type_value);
        uint16_t elem_len = *((uint16_t*)(unescape_buf_ + tempIndex));
        if (read_net_endian_) {
            elem_len = ntohs(elem_len);
        }
        tempIndex += sizeof(elem_len);
        uint16_t tag_len = *((uint16_t*)(unescape_buf_ + tempIndex));
        if (read_net_endian_) {
            tag_len = ntohs(tag_len);
        }
        tempIndex += sizeof(tag_len);

        if (tempIndex + tag_len > unescape_len_) {
            return false;
        }

        std::string tag(unescape_buf_ + tempIndex, tag_len);
        tempIndex += tag_len;
        elems_info_[tag] = std::pair<uint16_t, int>(type_value, tempIndex);
        index += elem_len;
    }

    been_parsed_ = true;

    return true;
}

}
