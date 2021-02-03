//
// Created by MarshallShuai on 2019-07-01.
//
#pragma once


#include <stdint.h>
#include <memory>
#include <file.h>

namespace kuaishou {
namespace cache {

class CacheContentV2WithScope;

class CacheScope final {
  public:
    CacheScope(int64_t start_position, std::shared_ptr<CacheContentV2WithScope> belonging_cache_content);

    std::shared_ptr<CacheContentV2WithScope> const& GetBelongingCacheContent() const;
    int64_t GetContentLength();

    /**
     *
     * @param content_length 整个文件content_length 必须大于0
     * @return origin cache_content
     */
    int64_t UpdateContentLength(int64_t content_length);

    int64_t GetStartPosition();

    /**
     * @return 最后一个有效字节的位置，而不是 offset after last byte
     */
    int64_t GetEndPosition();

    int64_t GetScopeMaxSize();

    /**
     *
     * @return 如果content-length确定了，则能返回确定的值，否则返回 kLengthUnset
     */
    int64_t GetActualLength();

    /*
     * 如果GetContentLength确定了，一般是有fileName的
     */
    std::string GetCacheFileName() const;

    /**
     * @return Scope的绝对文件路径
     */
    std::string GetCacheFilePath() const;

    /**
     * 从一个文件名恢复出scope信息，如果恢复失败，则返回nullptr
     */
    static std::shared_ptr<CacheScope> NewScopeFromFileName(const std::string& file_name, const std::string& dir_path);


    std::string GetKey() const;

    /**
     * 删除对应的文件
     * @param should_notify_file_manager 是否要通知 FileManager该分片文件被删除（为了统计已缓存字节数）
     * @return 删除的文件的大小
     */
    int64_t DeleteCacheFile(bool should_notify_file_manager = true) const;

    /**
     * 获取这个scope对应的缓存文件已经缓存的大小（因为一个scope可能是不完全缓存下来了的）
     * 这个是从CacheV2FileManager取的
     */
    int64_t GetCachedFileBytes() const;

    kpbase::File GetCacheFile() const;

    /**
     * 表示当前Scope代表的分片是否完全缓存到本地了s
     * 如果scope的start_pos/end_pos不正确，则也会返回false
     */
    bool IsScopeFulllyCached() const;

    bool IsLastScope() const;

  private:
    CacheScope() = delete;
    CacheScope& operator=(CacheScope&) = delete;
    /**
     * 生成分片对应的缓存文件的FileName
     * 这个必须是 belonging_cache_content_ 有有效值之后或者更新之后再调用
     */
    void UpdateFileName();

    /**
     * 根据规则找到scope的end position
     * 这个必须是 belonging_cache_content_ 有有效值之后或者更新之后再调用
     */
    void GenerateEndPosition();

  private:
    const int64_t start_position_;
    int64_t end_position_;
    bool is_last_scope_;    // 是否是最后一个分片

    std::shared_ptr<CacheContentV2WithScope> belonging_cache_content_;

    std::string file_name_;
    std::string file_path_;

};

} // namespace cache
} // namespace kuaishou
