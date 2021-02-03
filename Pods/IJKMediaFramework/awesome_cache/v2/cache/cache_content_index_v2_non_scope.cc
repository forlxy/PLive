//
// Created by MarshallShuai on 2019/10/16.
//

#include "cache_content_index_v2_non_scope.h"
#include "ac_log.h"

static constexpr bool kVerbose = false;

HODOR_NAMESPACE_START

CacheContentIndexV2NonScope::CacheContentIndexV2NonScope(
    std::string cache_dir_path) : CacheContentIndexV2<CacheContentV2NonScope>(cache_dir_path) {

}

std::shared_ptr<CacheContentV2NonScope>
CacheContentIndexV2NonScope::MakeCacheContent(std::string key, std::string dir_path, int64_t content_length) {
    return std::make_shared<CacheContentV2NonScope>(key, dir_path, content_length);
}


bool CacheContentIndexV2NonScope::CacheContentIntoDataStream(CacheContentV2NonScope& content,
                                                             kpbase::DataOutputStream& output_stream) {
    output_stream.WriteInt(content.SimpleHashCode());
    output_stream.WriteInt64(content.GetContentLength());
    output_stream.WriteString(content.GetKey());
    output_stream.WriteInt64(content.GetLastAccessTimestamp());

    return output_stream.Good();
}

std::shared_ptr<CacheContentV2NonScope> CacheContentIndexV2NonScope::CacheContentFromDataStream(kpbase::DataInputStream& input_stream) {
    int hash_code = input_stream.ReadInt();

    int ignore_err;
    int64_t content_length = input_stream.ReadInt64(ignore_err);

    std::string key = input_stream.ReadString();
    if (key.empty()) {
        LOG_ERROR("[CacheContentIndexV2NonScope::CacheContentFromDataStream] key is null!");
        return nullptr;
    }

    int64_t last_access_ts = input_stream.ReadInt64(ignore_err);
    // weak check, override to 0 if <0
    last_access_ts = std::max((int64_t)0, last_access_ts);

    if (kVerbose) {
        LOG_DEBUG("[CacheContentIndexV2NonScope::CacheContentFromDataStream], content_length:%lld:%lld, last_access_ts:%lld",
                  content_length, last_access_ts);
    }

    std::shared_ptr<CacheContentV2NonScope> ret_content;
    ret_content = MakeCacheContent(key, belonging_dir_path_, content_length);
    ret_content->SetLastAccessTimestamp(last_access_ts);

    if (ret_content->SimpleHashCode() != hash_code) {
        LOG_ERROR("[CacheContentIndexV2NonScope::CacheContentFromDataStream] ret_content->SimpleHashCode(%d) !=  hash_code(%d)",
                  ret_content->SimpleHashCode(), hash_code);
        return nullptr;
    }

    if (!input_stream.Good()) {
        LOG_ERROR("[CacheContentIndexV2NonScope::CacheContentFromDataStream] after all reading, !input_stream.Good()");
        return nullptr;
    }

    return ret_content;
}



HODOR_NAMESPACE_END
