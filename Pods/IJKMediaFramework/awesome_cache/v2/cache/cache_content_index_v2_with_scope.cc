//
// Created by MarshallShuai on 2019/10/16.
//

#include "ac_log.h"
#include "cache_content_index_v2_with_scope.h"
#include <utility>

HODOR_NAMESPACE_START

static constexpr bool kVerbose = false;

CacheContentIndexV2WithScope::CacheContentIndexV2WithScope(
    const std::string& cache_dir_path) : CacheContentIndexV2<CacheContentV2WithScope>(std::move(cache_dir_path)) {

}

std::shared_ptr<CacheContentV2WithScope>
CacheContentIndexV2WithScope::MakeCacheContent(std::string key, std::string dir_path, int64_t content_length) {
    return std::make_shared<CacheContentV2WithScope>(key, CacheV2Settings::GetScopeMaxSize(), dir_path, content_length);
}

bool CacheContentIndexV2WithScope::CacheContentIntoDataStream(CacheContentV2WithScope& content,
                                                              kpbase::DataOutputStream& output_stream) {
    output_stream.WriteInt(content.SimpleHashCode());
    output_stream.WriteInt64(content.GetContentLength());
    output_stream.WriteString(content.GetKey());
    output_stream.WriteInt64(content.GetScopeMaxSize());
    output_stream.WriteInt64(content.GetLastAccessTimestamp());

    return output_stream.Good();
}

std::shared_ptr<CacheContentV2WithScope> CacheContentIndexV2WithScope::CacheContentFromDataStream(kpbase::DataInputStream& input_stream) {
    int hash_code = input_stream.ReadInt();

    int ignore_err;
    int64_t content_length = input_stream.ReadInt64(ignore_err);

    std::string key = input_stream.ReadString();
    if (key.empty()) {
        LOG_ERROR("[CacheContentV2WithScope::CacheContentFromDataStream] key is null!");
        return nullptr;
    }

    int64_t scope_max_size = input_stream.ReadInt64(ignore_err);
    if (scope_max_size <= 0) {
        LOG_ERROR("[CacheContentV2WithScope::CacheContentFromDataStream] scope_max_size(%lld) <= 0!", scope_max_size);
        return nullptr;
    }

    int64_t last_access_ts = input_stream.ReadInt64(ignore_err);
    // weak check, override to 0 if <0
    last_access_ts = std::max((int64_t)0, last_access_ts);

    if (kVerbose) {
        LOG_DEBUG("[CacheContentV2WithScope::CacheContentFromDataStream], content_length:%lld, scope_max_size:%lld, last_access_ts:%lld",
                  content_length, scope_max_size, last_access_ts);
    }

    std::shared_ptr<CacheContentV2WithScope> ret_content;
    ret_content = MakeCacheContent(key, belonging_dir_path_, content_length);
    ret_content->SetLastAccessTimestamp(last_access_ts);

    if (ret_content->SimpleHashCode() != hash_code) {
        LOG_ERROR("[CacheContentV2WithScope::CacheContentFromDataStream] ret_content->SimpleHashCode(%d) !=  hash_code(%d)",
                  ret_content->SimpleHashCode(), hash_code);
        return nullptr;
    }

    if (!input_stream.Good()) {
        LOG_ERROR("[CacheContentV2WithScope::CacheContentFromDataStream] after all reading, !input_stream.Good()");
        return nullptr;
    }

    return ret_content;
}

std::vector<std::shared_ptr<CacheContentV2WithScope>>
CacheContentIndexV2WithScope::GetCacheContentListOfEvictStrategy(EvictStrategy strategy) {
    std::vector<std::shared_ptr<CacheContentV2WithScope>> ret_vec;
    std::lock_guard<std::mutex> lock(content_map_mutex_);
    for (auto iter = key_to_content_map_->begin(); iter != key_to_content_map_->end(); iter++) {
        if (iter->second.GetEvictStrategy() == strategy) {
            ret_vec.emplace_back(std::make_shared<CacheContentV2WithScope>(iter->second));
        }
    }

    return ret_vec;
}



HODOR_NAMESPACE_END
