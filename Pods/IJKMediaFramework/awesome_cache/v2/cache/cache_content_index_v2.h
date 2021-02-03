//
// Created by MarshallShuai on 2019-07-01.
//

#pragma once

#include <string>
#include <mutex>
#include <map>
#include "cache_content_v2.h"
#include <file.h>
#include "data_io_stream.h"

// for template impl
#include <ac_log.h>
#include <data_io_stream.h>
#include "io_stream.h"
#include "cache_v2_settings.h"
#include "cache_errors.h"
#include "cache_def_v2.h"
#include <algorithm>

namespace kuaishou {
namespace cache {

/**
 * Index文件会缓存到本地
 * todo 如果app启动的时候 恢复index文件失败了，会负责删除文件
 * 所有的public接口是线程安全的
 *
 */

static const std::string kCacheV2IndexFileName = "cached_content_index.v2.2.aci";
static const std::string kCacheV2IndexBackupFileName = "cached_content_index.v2.2.aci.bak";

template <typename T>
class CacheContentIndexV2 {
  public:
    CacheContentIndexV2(std::string cache_dir_path);
    /**
     * 如果Index里保存了相关CacheContent的信息，则返回
     * 目前这个函数默认调用方只有dataSource，所以一旦调用就会更新CacheContent的timestamp
     * @param key 视频key
     * @return CacheContent
     */
    std::shared_ptr<T> GetCacheContent(const std::string& url, std::string key, bool update_timestamp = false);

    /**
     * 查找key对应的CacheContent
     */
    std::shared_ptr<T> FindCacheContent(std::string key);

    void PutCacheContent(const std::shared_ptr<T>& content);

    /**
     * 删除CacheContent，删除之前确认timeStamp没更新过。
     * 本接口不负责删除CacheContent对应的文件，只负责删除Index里的相关记录
     * @param content CacheContent句柄
     * @param force 一般来说，如果CacheContentV2的时间戳变化了（可能被别人update过timestamp)，那force为false的时候不会强制删除
     *              如果想强制删除，则需要传入true
     */
    void RemoveCacheContent(std::shared_ptr<T> content, bool force = false);

    /**
     * 把所有CachedBytes为0的CacheContent，以及content-length <= 0 的cache_content(目前来说，
     * 只有单元测试才会往index里insert content-length <=0的content)
     */
    int RemoveAllEmptyCacheContent();

    /**
     * 把内存里的表现形式 flush到本地的index文件
     * 分两步，
     * @return 成功与否
     */
    bool Store();

    /**
     * 从本地Index文件恢复
     * 内部不检测曾经是否Load过
     *
     * @return 成功与否
     */
    bool Load();

    /**
     * 至少load过一次
     */
    bool IsLoaded();

    std::vector<std::shared_ptr<T>> GetCacheContentList();

    /**
     * 当前index里存的CacheContent个数
     * @return CacheContent个数
     */
    int GetCacheContentCount();

    /**
     * 清空Index内部的内存状态
     */
    void Reset();
    /**
     * 只尝试解析文件，如果失败，不负责清理文件的工作
     * @return 是否Load成功
     */
    bool TryLoadFromFile(kpbase::File& file);

    /**
     * 只尝试flush到本地文件，如果失败，不负责清理文件的工作
     * @return 是否Load成功
     */
    bool TryStoreIntoFile(kpbase::File& file);

    /**
     * only for unit test
     */
    void ClearAllIndexFiles();

    void RemoveEmptyCacheContent(std::function<bool(const std::string& key)> should_remove_lambda);

  protected:
    /**
     *
     * @param content CacheContent句柄
     * @param output_stream DataOutputStream
     * @return CacheContent写入DataStream成功
     */
    virtual bool CacheContentIntoDataStream(T& content, kpbase::DataOutputStream& output_stream) = 0;
    /**
     *
     * @param input_stream DataInputStream
     * @return 败返回包含null的智能指针
     */
    virtual std::shared_ptr<T>  CacheContentFromDataStream(kpbase::DataInputStream& input_stream) = 0;

    virtual std::shared_ptr<T> MakeCacheContent(std::string key, std::string dir_path, int64_t content_length = 0) = 0;

  protected:
    std::string belonging_dir_path_;

    // 这里不存为shared_ptr，防止传出去的CacheContentV2被外部更改，
    std::shared_ptr<std::map<std::string, T>> key_to_content_map_;
    std::mutex content_map_mutex_;

    bool loaded_before_;
};

// 因为模板的实现需要放在头文件，所以暂时定义了一个特别具体的verbose开关
#define kCacheContentIndexV2Verbose false

template <typename T>
CacheContentIndexV2<T>::CacheContentIndexV2(std::string cache_dir_path) {
    key_to_content_map_ = std::make_shared<std::map<std::string, T>>();
    belonging_dir_path_ = cache_dir_path;
    loaded_before_ = false;
}

template <typename T>
bool CacheContentIndexV2<T>::Store() {
    std::lock_guard<std::mutex> lock(content_map_mutex_);
    kpbase::File index_file(belonging_dir_path_, kCacheV2IndexFileName);
    kpbase::File index_bak_file(belonging_dir_path_, kCacheV2IndexBackupFileName);

    // 如果正式index存在，则先改名为bak
    if (index_bak_file.Exists()) {
        index_bak_file.Remove();
    }
    index_file.RenameTo(index_bak_file);

    // 更新到正式文件
    bool ret = TryStoreIntoFile(index_file);
    if (!ret) {
        // 如果失败，则把bak变回来，返回false
        LOG_ERROR("[CacheContentIndexV2::Store]TryStoreIntoFile fail");
        ret = index_bak_file.RenameTo(index_file);
        if (!ret) {
            // 失败了没关系，反正load的时候还会做正确性校验，不过日志还是要打一个的
            LOG_ERROR("[CacheContentIndexV2::Store]index_bak_file.RenameTo(index_file) fail ：<");
        }
        return false;
    } else {
        // 如果成功，删除bak文件，返回true
        index_bak_file.Remove();

        if (kCacheContentIndexV2Verbose) {
            LOG_DEBUG("[CacheContentIndexV2::Store] success");
        }
        return true;
    }
}

template <typename T>
bool CacheContentIndexV2<T>::Load() {
    std::lock_guard<std::mutex> lock(content_map_mutex_);
    kpbase::File index_file(belonging_dir_path_, kCacheV2IndexFileName);
    kpbase::File index_bak_file(belonging_dir_path_, kCacheV2IndexBackupFileName);

    key_to_content_map_->clear();

    // try 正式文件
    bool ret = TryLoadFromFile(index_file);
    if (ret) {
        LOG_DEBUG("[CacheContentIndexV2::Load]load index_file success");
    } else {
        // 不行再try backup文件
        ret = TryLoadFromFile(index_bak_file);
        if (ret) {
            index_file.Remove();
            index_bak_file.RenameTo(index_file);
            LOG_DEBUG("[CacheContentIndexV2::Load]load index_bak_file success");
        } else {
            // 如果成功了就染回true，如果都失败了，就把2个文件都删了
            index_file.Remove();
            index_bak_file.Remove();
            LOG_WARN("[CacheContentIndexV2::Load]remove all index file, success");
        }
    }

    loaded_before_ = true;
    return true;

}

template <typename T>
bool CacheContentIndexV2<T>::IsLoaded() {
    return loaded_before_;
}


template <typename T>
std::vector<std::shared_ptr<T>> CacheContentIndexV2<T>::GetCacheContentList() {
    std::lock_guard<std::mutex> lock(content_map_mutex_);

    std::vector<std::shared_ptr<T>> ret_vec;
    for (auto iter = key_to_content_map_->begin(); iter != key_to_content_map_->end(); iter++) {
        ret_vec.emplace_back(std::make_shared<T>(iter->second));
    }
    return ret_vec;
}

template <typename T>
int CacheContentIndexV2<T>::GetCacheContentCount() {
    std::lock_guard<std::mutex> lock(content_map_mutex_);
    return key_to_content_map_->size();
}

template <typename T>
void CacheContentIndexV2<T>::Reset() {
    key_to_content_map_->clear();
}

template <typename T>
std::shared_ptr<T> CacheContentIndexV2<T>::GetCacheContent(const std::string& url, std::string key, bool update_timestamp) {
    std::lock_guard<std::mutex> lg(content_map_mutex_);

    if (key.empty()) {
        key = CacheContentV2::GenerateKey(url);
        // 这个warn不是真正的warn，不过在app的场景还是值得warn的，因为app是要求必须输入key的
        LOG_WARN("[CacheContentIndexV2::GetCacheContent] input key is empty ,try find GenerateKey(key):%s",
                 key.c_str());
    }
    auto iter = key_to_content_map_->find(key);
    std::shared_ptr<T> ret_content;
    if (key_to_content_map_->end() != iter) {
        if (kCacheContentIndexV2Verbose) {
            LOG_DEBUG("[CacheContentIndexV2::GetCacheContent]find cache_content for key:%s", key.c_str());
        }

        ret_content = std::make_shared<T>(iter->second);
        // 更新访问时间戳
        if (update_timestamp) {
            ret_content->UpdateLastAccessTimestamp();
        }
    } else {
        ret_content = MakeCacheContent(key, belonging_dir_path_);
    }

    return ret_content;
}


template <typename T>
std::shared_ptr<T> CacheContentIndexV2<T>::FindCacheContent(std::string key) {
    std::lock_guard<std::mutex> lg(content_map_mutex_);
    if (key.empty()) {
        LOG_WARN("[CacheContentIndexV2::FindCacheContent] input key is null, invalid, return");
        return nullptr;
    }
    auto iter = key_to_content_map_->find(key);
    if (key_to_content_map_->end() != iter) {
        return std::make_shared<T>(iter->second);
    } else {
        LOG_WARN("[CacheContentIndexV2<T>::FindCacheContent]find by key:%s found null", key.c_str());
        return nullptr;
    }
}

template <typename T>
void CacheContentIndexV2<T>::PutCacheContent(const std::shared_ptr<T>& content) {
    std::lock_guard<std::mutex> lg(content_map_mutex_);
    auto key = content->GetKey();
    if (key.empty()) {
        LOG_ERROR("[CacheContentIndexV2::PutCacheContent] try to insert empty key, url:%s");
        return;
    } else {
        if (kCacheContentIndexV2Verbose) {
            LOG_DEBUG("[CacheContentIndexV2::PutCacheContent] insert cache_content for key :%s",
                      content->GetKey().c_str());
        }
    }
    auto insert_ret = key_to_content_map_->emplace(key, *content);
    if (!insert_ret.second) {
        T& origin_content = key_to_content_map_->find(key)->second;
        if (content->GetContentLength() != origin_content.GetContentLength()) {
            LOG_ERROR("[CacheContentIndexV2::PutCacheContent]content length not equals between old(%lld) and new(%lld), will override old one",
                      origin_content.GetContentLength(), content->GetContentLength());
        }
        // override it
        insert_ret.first->second.SetContentLength(content->GetContentLength());
    }
}

template <typename T>
void CacheContentIndexV2<T>::RemoveCacheContent(std::shared_ptr<T> content, bool force) {
    std::lock_guard<std::mutex> lg(content_map_mutex_);
    auto iter = key_to_content_map_->find(content->GetKey());
    if (iter == key_to_content_map_->end()) {
        // 现在的流程不应该在RemoveStaleCacheContent的时候找不到，这里做弱检测，只warn一下
        // 单元测试可能会出现，正常流程不应该出现
        LOG_WARN("[CacheContentIndexV2::RemoveCacheContent]warning content not found for key:%s",
                 content->GetKey().c_str());
    } else {
        if (iter->second.GetLastAccessTimestamp() != content->GetLastAccessTimestamp() && !force) {
            LOG_INFO("[CacheContentIndexV2::RemoveCacheContent] content timestamp updated, not to remove");
        } else {
            key_to_content_map_->erase(iter);
            if (kCacheContentIndexV2Verbose) {
                LOG_VERBOSE("[CacheContentIndexV2::RemoveCacheContent] content removed, size:%.1f, ts:%lld, key:%s",
                            content->GetContentLength() * 1.f / MB, content->GetLastAccessTimestamp(), content->GetKey().c_str());
            }
        }
    }
}

template <typename T>
int CacheContentIndexV2<T>::RemoveAllEmptyCacheContent() {
    std::vector<std::shared_ptr<T>> to_remove_list;
    auto cur_list = GetCacheContentList();

    for (auto content : cur_list) {
        if (content->GetContentLength() <= 0 || content->GetCachedBytes() == 0) {
            to_remove_list.push_back(content);
        }
    }

    {
        std::lock_guard<std::mutex> lg(content_map_mutex_);
        for (auto content : to_remove_list) {
            auto iter = key_to_content_map_->find((content->GetKey()));
            if (iter != key_to_content_map_->end()) {
                key_to_content_map_->erase(iter);
            }
        }
        if (kCacheContentIndexV2Verbose) {
            LOG_INFO("[CacheContentIndexV2::RemoveAllEmptyCacheContent] %d cache_content removed", to_remove_list.size());
        }
    }
    return static_cast<int>(to_remove_list.size());
}

template <typename T>
bool CacheContentIndexV2<T>::TryLoadFromFile(kpbase::File& file) {
    if (!file.Exists()) {
        LOG_WARN("[CacheContentIndexV2::TryLoadFromFile]file not exist:%s", file.path().c_str());
        return false;
    }

    kpbase::DataInputStream input(file);
    if (!input.Good()) {
        LOG_ERROR("[CacheContentIndexV2::TryLoadFromFile] input broke at first, file:%s", file.path().c_str());
        return kResultCachedContentIndexStoreOutputBroken;
    }
    int content_cnt = input.ReadInt();

    std::shared_ptr<std::map<std::string, T>> result_map = std::make_shared<std::map<std::string, T>>();

    int64_t hash_code = 0;
    for (int i = 0; i < content_cnt; i++) {
        auto content = CacheContentFromDataStream(input);
        if (nullptr == content) {
            LOG_ERROR("[CacheContentIndexV2::TryLoadFromFile] CacheContentFromDataStream fail");
            return false;
        }
        if (kCacheContentIndexV2Verbose) {
            LOG_DEBUG("[CacheContentIndexV2::TryLoadFromFile]hash_code(%lld) += content->SimpleHashCode(%d) = :%lld",
                      hash_code, content->SimpleHashCode(), hash_code + content->SimpleHashCode())
        }
        hash_code += content->SimpleHashCode();
        // fixme 这块加个容错逻辑？比如两个content冲突了，做一个2选一删除逻辑，目前采用insert，只保留第一个遇到的
        result_map->emplace(content->GetKey(), *content);
    }

    int err = 0;
    int64_t read_hash_code = input.ReadInt64(err);
    if (read_hash_code != hash_code) {
        LOG_ERROR("[CacheContentIndexV2::TryLoadFromFile] read_hash_code(%lld) !=  hash_code(%lld), err:%d",
                  read_hash_code, hash_code, err);
        return false;
    }

    if (kCacheContentIndexV2Verbose) {
        LOG_DEBUG("[CacheContentIndexV2::TryLoadFromFile] load success, from file:%s", file.path().c_str());
    }
    key_to_content_map_ = result_map;
    return true;
}

template <typename T>
bool CacheContentIndexV2<T>::TryStoreIntoFile(kpbase::File& file) {
    kpbase::DataOutputStream output(file);
    if (!output.Good()) {
        LOG_ERROR("[CacheContentIndexV2::TryStoreIntoFile] outputStream broke at first");
        return kResultCachedContentIndexStoreOutputBroken;
    }
    output.WriteInt((int)key_to_content_map_->size());
    int64_t hash_code = 0;
    bool b_ret;
    for (auto it : (*key_to_content_map_)) {
        b_ret = CacheContentIntoDataStream(it.second, output);
        if (!b_ret) {
            LOG_ERROR("[CacheContentIndexV2::TryStoreIntoFile] outputStream broke after CacheContentIntoDataStream");
            return false;
        }
        int content_hash = it.second.SimpleHashCode();
        hash_code += content_hash;
    }

    int writeErr = output.WriteInt64(hash_code);
    if (writeErr != 0) {
        LOG_ERROR("[CacheContentIndexV2::TryStoreIntoFile] output.WriteInt64(hash_code), error:%d", writeErr);
        return false;
    }
    if (!output.Good()) {
        LOG_ERROR("[CacheContentIndexV2::TryStoreIntoFile] outputStream broke after write hashCode");
        return false;
    } else {
        return true;
    }
}


template <typename T>
void CacheContentIndexV2<T>::ClearAllIndexFiles() {
    std::lock_guard<std::mutex> lock(content_map_mutex_);
    kpbase::File index_file(belonging_dir_path_, kCacheV2IndexFileName);
    kpbase::File index_bak_file(belonging_dir_path_, kCacheV2IndexBackupFileName);
    index_file.Remove();
    index_bak_file.Remove();
}

template <typename T>
void CacheContentIndexV2<T>::RemoveEmptyCacheContent(
    std::function<bool(const std::string& key)> should_remove_lambda) {
    std::lock_guard<std::mutex> lock(content_map_mutex_);
    for (auto iter = key_to_content_map_->begin(); iter != key_to_content_map_->end();) {
        auto& cc = (*iter).second;
        if (should_remove_lambda(cc.GetKey())) {
            LOG_INFO("[CacheContentIndexV2WithScope::RemoveEmptyCacheContent]key:%s, contentLength:lld",
                     cc.GetKey().c_str(), cc.GetContentLength());
            iter = key_to_content_map_->erase(iter);
        } else {
            iter++;
        }
    }
}

} // namespace cache
} // namespace kuaishou
