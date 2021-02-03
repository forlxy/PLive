//
// Created by MarshallShuai on 2019-10-16.
//

#pragma once

#include "utils/macro_util.h"
#include "cache_content_v2.h"
#include "v2/cache/cache_v2_settings.h"

HODOR_NAMESPACE_START

#define kCacheContentFileNameFormatSuffix "v2.1.acc" // 文件名必须以这个字符串结尾，不然不符合版本，每次改了文件格式，都要升级acc前面的数字
#define kCacheContentFileNameFormatWithoutSuffix "%lld_%s.v2.1" // contentLength_key
#define kCacheContentFileNameFormat  kCacheContentFileNameFormatWithoutSuffix "." kCacheContentFileNameFormatSuffix

class CacheContentV2NonScope final: public CacheContentV2 {
  public:

    CacheContentV2NonScope(std::string key, std::string cache_dir_path = CacheV2Settings::GetResourceCacheDirPath(), int64_t content_length = 0);

    virtual int64_t GetCachedBytes(bool force_check_file = false) const override;

    static std::shared_ptr<CacheContentV2NonScope> NewCacheContentFromFileName(const std::string& file_name,
                                                                               const std::string& cache_dir);

    std::string GetCacheContentFileName() const ;

    /**
     * 获取该文件的路径，如果当前文件名还不确定（比如还没拿到contentlength的时候），则返回空字符串
     */
    std::string GetCacheContentValidFilePath() const;

    kpbase::File GetCacheContentCacheFile() const;
    /**
     * 删除对应的文件
     * @param should_notify_file_manager 是否要通知 FileManager该分片文件被删除（为了统计已缓存字节数）
     * @return 删除的文件的大小
     */
    int64_t DeleteCacheContentFile(bool should_notify_file_manager = true) const;

    void UpdateContentLengthAndFileName(int64_t content_length);

  private:
    void UpdateFileName();

  private:
    std::string file_name_;
};

HODOR_NAMESPACE_END
