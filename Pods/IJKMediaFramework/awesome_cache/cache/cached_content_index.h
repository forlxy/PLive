//
// Created by 帅龙成 on 30/10/2017.
//
#pragma once

#include <string>
#include <set>
#include <map>
#include <vector>
#include "cached_content.h"
#include "data_io_stream.h"
#include "atomic_file.h"

namespace kuaishou {
namespace cache {
class CachedContentIndex {
  public:
    static const std::string kFileName;
  public:
    CachedContentIndex(kpbase::File cache_dir);

    /** Loads the index file. */
    void Load();

    /** Stores the index data to index file if there is a change. */
    AcResultType Store();

    std::shared_ptr<CachedContent> Add(const std::string& key);

    /** Returns a CachedContent instance with the given key or null if there isn't one. */
    std::shared_ptr<CachedContent> Get(const std::string& key);

    std::vector<std::shared_ptr<CachedContent>> GetAll() const;

    /** Returns an existing or new id assigned to the given key. */
    int AssignIdForKey(const std::string& key);

#ifdef TESTING
    virtual
#endif
    std::string GetKeyForId(int id);

    void RemoveEmpty(std::set<std::string> locked_keys);

    /**
     * Removes {@link CachedContent} with the given key from index. It shouldn't contain any spans.
     */
    bool RemoveEmpty(const std::string& key);

    /** Removes empty {@link CachedContent} instances from index. */
    void RemoveEmptyAndInvalid();

    std::set<std::string> GetKeys() const;

    void SetContentLength(const std::string& key, int64_t length);

    int64_t GetContentLength(const std::string& key);

    int64_t GetContentCachedBytes(const std::string& key);

    void AddNewCachedContent(std::shared_ptr<CachedContent> cached_content);

  private:
    void ResetOnFileIoError();
    bool ReadFile();
    AcResultType WriteFile();
    void AddCachedContent(std::shared_ptr<CachedContent> cached_content);
    std::shared_ptr<CachedContent> CreateAndAddCachedContent(std::string key, int64_t length);
    int GetNewId();

    static const int kVersion = 1;
    std::map<std::string, std::shared_ptr<CachedContent>> key_to_content_;
    std::set<int> ids_;
    std::map<int, std::string> id_to_key_;
    std::unique_ptr<kpbase::AtomicFile> index_file_;
    kpbase::File cache_dir_;
    bool changed_;
};
}
}
