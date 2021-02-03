//
// Created by 帅龙成 on 30/10/2017.
//

#pragma once

#include "include/data_spec.h"

namespace kuaishou {
namespace cache {

struct CacheUtil {
    static std::string GetKey(const DataSpec& spec);

    static std::string GenerateKey(std::string uri);
    //缓存文件系统出错，包括打开、读取、写入异常
    static bool IsFileSystemError(int64_t error);
    //文件读取时出现IO错误
    static bool IsFileReadError(int64_t error);

    /**
     *
     * @param use_timestamp_max_len 加入时间戳作为一个随机变量因子，默认为6位，
     * 目前在mac单元测试里，如果不加时间戳因子，则32位的碰撞率大概在万分之一
     * ，加了的话（使用len=6），本地1000w测试无碰撞
     */
    static std::string GenerateUUID(int use_timestamp_max_len = 6);

    /**
     * 读取文件到buf
     * @param file_path 文件路径
     * @param buf 读取数据buf
     * @param len buf长度
     */
    static int64_t ReadFile(const std::string& file_path, uint8_t* buf, int64_t len);
};

}
}
