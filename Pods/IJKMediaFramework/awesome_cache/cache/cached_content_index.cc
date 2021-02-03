//
// Created by 帅龙成 on 30/10/2017.
//

#include "ac_log.h"
#include "cached_content_index.h"

namespace kuaishou {
namespace cache {

const std::string CachedContentIndex::kFileName = "cached_content_index.aci";

CachedContentIndex::CachedContentIndex(kpbase::File cache_dir) :
    index_file_(new kpbase::AtomicFile(kpbase::File(cache_dir, kFileName))),
    cache_dir_(cache_dir),
    changed_(false) {
}

void CachedContentIndex::Load() {
    assert(!changed_);
    if (!ReadFile()) {
        ResetOnFileIoError();
    }
}

void CachedContentIndex::ResetOnFileIoError() {
    index_file_->Remove();
    key_to_content_.clear();
    ids_.clear();
    id_to_key_.clear();
    changed_ = false;
}

std::shared_ptr<CachedContent> CachedContentIndex::Get(const std::string& key) {
    return key_to_content_.count(key) > 0 ? key_to_content_[key] : nullptr;
}

AcResultType CachedContentIndex::Store() {
    if (!changed_ && kpbase::File(cache_dir_, kFileName).Exists()) {
        return kResultOK;
    }

    AcResultType ret = WriteFile();
    if (ret != kResultOK) {
        LOG_ERROR_DETAIL("CachedContentIndex::Store, fail");
        ResetOnFileIoError();
        return ret;
    }

    changed_ = false;
    return kResultOK;
}

std::shared_ptr<CachedContent> CachedContentIndex::Add(const std::string& key) {
    std::shared_ptr<CachedContent> content = key_to_content_[key];
    if (!content) {
        content = CreateAndAddCachedContent(key, kLengthUnset);
    }
    return content;
}

std::vector<std::shared_ptr<CachedContent>> CachedContentIndex::GetAll() const {
    std::vector<std::shared_ptr<CachedContent> > ret;
    for (auto it : key_to_content_) {
        ret.push_back(it.second);
    }
    return ret;
}

std::string CachedContentIndex::GetKeyForId(int id) {
    return id_to_key_.find(id) != id_to_key_.end() ? id_to_key_[id] : "";
}

void CachedContentIndex::RemoveEmptyAndInvalid() {
    std::vector<std::shared_ptr<CachedContent> > to_be_removed;
    for (auto it : key_to_content_) {
        if (it.second->IsEmptyOrInValid()) {
            to_be_removed.push_back(it.second);
        }
    }
    for (auto& cached_content : to_be_removed) {
        RemoveEmpty(cached_content->key());
    }
}

bool CachedContentIndex::RemoveEmpty(const std::string& key) {
    auto content_it = key_to_content_.find(key);
    if (content_it != key_to_content_.end()) {
        assert(content_it->second->IsEmptyOrInValid());
        id_to_key_.erase(content_it->second->id());
        ids_.erase(content_it->second->id());
        key_to_content_.erase(content_it);
        changed_ = true;
        return true;
    } else {
        return false;
    }
}

void CachedContentIndex::RemoveEmpty(std::set<std::string> locked_keys) {
    std::vector<std::shared_ptr<CachedContent> > to_be_removed;
    for (auto it : key_to_content_) {
        if (locked_keys.find(it.first) == locked_keys.end() && it.second->IsEmptyOrInValid()) {
            to_be_removed.push_back(it.second);
        }
    }
    for (auto& cached_content : to_be_removed) {
        RemoveEmpty(cached_content->key());
    }
}

int CachedContentIndex::AssignIdForKey(const std::string& key) {
    return Add(key)->id();
}

std::set<std::string> CachedContentIndex::GetKeys() const {
    std::set<std::string> ret;
    for (auto it : key_to_content_) {
        ret.insert(it.first);
    }
    return ret;
}

void CachedContentIndex::SetContentLength(const std::string& key, int64_t length) {
    auto cached_content = Get(key);
    if (cached_content) {
        if (cached_content->length() != length) {
            cached_content->SetLength(length);
            changed_ = true;
        }
    } else {
        CreateAndAddCachedContent(key, length);
    }
}

int64_t CachedContentIndex::GetContentLength(const std::string& key) {
    auto cached_content = Get(key);
    return cached_content == nullptr ? kLengthUnset : cached_content->length();
}

int64_t CachedContentIndex::GetContentCachedBytes(const std::string& key) {
    auto cached_content = Get(key);
    return cached_content == nullptr ? kLengthUnset : cached_content->cached_bytes();
}

bool CachedContentIndex::ReadFile() {
    auto file_input_stream = index_file_->StartRead();
    auto input = std::unique_ptr<kpbase::DataInputStream>(new kpbase::DataInputStream(*(file_input_stream.get())));
    if (!input->Good()) {
        LOG_ERROR_DETAIL("CachedContentIndex::ReadFile !input->Good()");
        return false;
    }
    int version = input->ReadInt();
    if (version != kVersion) {
        LOG_ERROR_DETAIL("CachedContentIndex::ReadFile version(%d) != kVersion(%d)", version, kVersion);
        return false;
    }
    int count = input->ReadInt();
    int64_t hashCode = 0;
    for (int i = 0; i < count; ++i) {
        std::shared_ptr<CachedContent> cached_content = CachedContent::NewFromDataInputStream(input.get());
        if (cached_content == nullptr) {
            LOG_ERROR_DETAIL("CachedContentIndex::ReadFile NewFromDataInputStream fail");
            // 一旦遇到一次解析失败，则本次content index均作废
            return false;
        }
        AddCachedContent(cached_content);
        int content_hash = cached_content->HeaderHashCode();

        hashCode += content_hash;
    }

    int err = 0;
    int64_t inputHashCode = input->ReadInt64(err);

    if (err != 0) {
        LOG_ERROR_DETAIL("[CachedContentIndex::ReadFile] ReadInt64 for inputHashCode fail, err:%d", err);
        return false;
    }


    if (inputHashCode != hashCode) {
        LOG_ERROR_DETAIL("CachedContentIndex::ReadFile inputHashCode(%lld) != hashCode(%lld)", inputHashCode, hashCode);
        return false;
    }

    return true;
}

AcResultType CachedContentIndex::WriteFile() {
    auto file_output_stream = index_file_->StartWrite();
    if (file_output_stream == nullptr) {
        LOG_ERROR_DETAIL("[CachedContentIndex] index_file_->StartWrite() fail");
        return kResultCachedContentIndexStoreStartWriteFail;
    }
    auto output = std::unique_ptr<kpbase::DataOutputStream>(new kpbase::DataOutputStream(*(file_output_stream.get())));
    if (!output->Good()) {
        return kResultCachedContentIndexStoreOutputBroken;
    }
    output->WriteInt(kVersion);
    output->WriteInt((int)key_to_content_.size());
    int64_t hash_code = 0;
    AcResultType ret = kResultOK;
    for (auto it : key_to_content_) {
        ret = it.second->WriteToStream(output.get());
        if (ret != kResultOK) {
            return ret;
        }
        int content_hash = it.second->HeaderHashCode();
        hash_code += content_hash;
    }

    output->WriteInt64(hash_code);
    if (!output->Good()) {
        return kResultCachedContentIndexStoreOutputBroken_2;
    }

    index_file_->EndWrite(std::move(file_output_stream));
    return kResultOK;
}

void CachedContentIndex::AddNewCachedContent(std::shared_ptr<CachedContent> cached_content) {
    AddCachedContent(cached_content);
    changed_ = true;
}

void CachedContentIndex::AddCachedContent(std::shared_ptr<CachedContent> cached_content) {
    key_to_content_[cached_content->key()] = cached_content;
    ids_.insert(cached_content->id());
    id_to_key_[cached_content->id()] = cached_content->key();
}

std::shared_ptr<CachedContent> CachedContentIndex::CreateAndAddCachedContent(std::string key, int64_t length) {
    int id = GetNewId();
    std::shared_ptr<CachedContent> cached_content = std::make_shared<CachedContent>(id, key, length);
    AddNewCachedContent(cached_content);
    return cached_content;
}

int CachedContentIndex::GetNewId() {
    int id = ids_.size() == 0 ? 0 : (*ids_.rbegin() + 1);
    // in case if we pass max int value.
    if (id < 0) {
        for (id = 0; id < ids_.size(); ++id) {
            if (ids_.find(id) == ids_.end()) {
                break;
            }
        }
    }
    return id;
}

}
}
