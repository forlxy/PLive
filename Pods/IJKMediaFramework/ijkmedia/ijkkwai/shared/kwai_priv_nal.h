#pragma once

#include <stdint.h>
#include <string>
#include <map>
#include <libavcodec/avcodec.h>

namespace KWAI {

class PrivNal {
  public:
    PrivNal();
    PrivNal(const char* buf, const int len, enum AVCodecID codecId, bool copyMem = false); // for parsing usage
    virtual ~PrivNal();

  public:
    bool isValid() const { return is_valid_; }
    char* getNalData() { return nal_buf_; }
    int getNalLen() const { return nal_len_; }

    bool writeElemBuf(const std::string& tag, const char* buf, const int len);
    bool writeElemString(const std::string& tag, const std::string& value);
    bool writeElemInt32(const std::string& tag, int32_t value);
    bool writeElemInt64(const std::string& tag, int64_t value);
    bool writeElemFloat(const std::string& tag, float value);
    bool writeElemDouble(const std::string& tag, double value);

    bool getElemBuf(const std::string& tag, char* buf, int& len);
    bool getElemString(const std::string& tag, std::string& value);
    bool getElemInt32(const std::string& tag, int32_t& value);
    bool getElemInt64(const std::string& tag, int64_t& value);
    bool getElemFloat(const std::string& tag, float& value);
    bool getElemDouble(const std::string& tag, double& value);

  private:
    union FourBytes {
        int32_t value_int32;
        float value_float;
    };
    union EightBytes {
        int64_t value_int64;
        double value_double;
    };
    enum PrivNalElemType {
        TYPE_BUF = 1,
        TYPE_STRING,
        TYPE_INT32,
        TYPE_INT64,
        TYPE_FLOAT,
        TYPE_DOUBLE
    };
    bool writeElem(const std::string& tag, PrivNalElemType type, const char* value, int valueLen);
    int getElemIndex(const std::string& tag, PrivNalElemType type);
    bool parse();

  private:
    static const uint32_t PRIV_NAL_MAGIC_H264 = 0x1F4B531F; // 0x1F: nalType 31
    static const uint32_t PRIV_NAL_MAGIC_HEVC = 0x7E01017E; // 0x7E: nalType 63; 0x01: layerId 0, tid 1
    static const uint16_t PRIV_NAL_HEADER_LEN = 10; // startCode(4 bytes) + magic(4 bytes) + nalLen(2 bytes)
    static const int PRIV_NAL_MAX_LEN = 1024;
    bool is_valid_;
    char* nal_buf_;
    uint16_t nal_len_;
    bool is_buf_alloc_;
    bool been_parsed_;
    char* unescape_buf_;
    int unescape_len_;
    std::map<std::string, std::pair<uint16_t, int> > elems_info_; // key: tag; value: <typeValue, dataIndex>
    bool read_net_endian_; // hotfix for Endian issue: to support parsing fixed version
};

};

