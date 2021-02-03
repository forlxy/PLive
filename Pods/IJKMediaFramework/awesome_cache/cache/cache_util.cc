//
// Created by 帅龙成 on 30/10/2017.
//

#include <sstream>
#include <utility.h>
#include "io_stream.h"
#include "cache_util.h"
#include "file.h"
#include "ac_log.h"


namespace kuaishou {
namespace cache {

std::string CacheUtil::GetKey(const DataSpec& spec) {
    return !spec.key.empty() ? spec.key : GenerateKey(spec.uri);
}

std::string CacheUtil::GenerateKey(std::string uri) {
    return uri;
}

bool CacheUtil::IsFileSystemError(int64_t error) {
    switch (error) {
        case kResultCacheExceptionTouchSpan:
        case kResultFileExceptionCreateDirFail:
        case kResultFileExceptionIO:

        case kResultAdvanceDataSinkCloseFlushFail:
        case kResultAdvanceDataSinkWriteFail:
        case kResultCachedContentWriteToStreamFail:

        case kResultFileDataSourceIOError_0:
        case kResultFileDataSourceIOError_1:
        case kResultFileDataSourceIOError_2:
        case kResultFileDataSourceIOError_3:
        case kResultFileDataSourceIOError_4:

        case kResultCachedContentIndexStoreStartWriteFail:
        case kResultCachedContentIndexStoreOutputBroken:
        case kResultCachedContentIndexStoreOutputBroken_2:
            return true;

        default:
            return false;
    }
    return false;
}

bool CacheUtil::IsFileReadError(int64_t error) {
    switch (error) {
        case kResultFileExceptionCreateDirFail:
        case kResultFileDataSourceIOError_3:
            return true;
        default:
            return false;
    }
    return false;
}

std::string CacheUtil::GenerateUUID(int use_timestamp_max_len) {
    static const int OUTPUT_TOTAL_LEN = 32;
    static std::string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    kuaishou::kpbase::UniformRandomIntGenerator<int> rand(0, (int)alphabet.size() - 1);
    std::stringstream ss;

    size_t time_str_len = 0;
    // microsecond是比较好的一个维度，如果用 nanoseconds 或者 milliseconds 的话，后4位变化频率比较低
    if (use_timestamp_max_len > 0) {
        auto time = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();
        std::string time_str = std::to_string(time);
        time_str_len = time_str.size() >= use_timestamp_max_len ? use_timestamp_max_len : time_str.size();
        auto time_sub_str = time_str.substr(time_str.size() - time_str_len);
        ss << time_sub_str;
    }

    for (int i = 0; i < OUTPUT_TOTAL_LEN - time_str_len; ++i) {
        ss << alphabet.at(static_cast<unsigned long>(rand.Next()));
    }

    return ss.str();
}

int64_t CacheUtil::ReadFile(const std::string& file_path, uint8_t* buf, int64_t len) {
    if (file_path.empty()) {
        return kCacheUtilFilePathEmpty;
    }
    kpbase::File file = kpbase::File(file_path);
    if (!file.Exists()) {
        return kCacheUtilFileNotExist;
    }

    auto file_length = file.file_size();
    if (file_length < 0) {
        LOG_ERROR_DETAIL("[CacheUtil::ReadDataFromCacheFile]file_length invalid:%lld, path:%s",
                         file_length, file.path().c_str());
        file.Remove();
        return kCacheUtilReadOverflow;
    }

    auto input_stream = kpbase::InputStream(file);
    auto acutal_read_len = input_stream.Read(buf, 0, len);

    if (acutal_read_len < 0) {
        LOG_ERROR_DETAIL("[CacheUtil::ReadDataFromCacheFile] input_stream.Read, error:%lld, path:%s",
                         acutal_read_len, file.path().c_str());
        file.Remove();
        return kCacheUtilReadFail;
    }
    return acutal_read_len;
}


}
}
