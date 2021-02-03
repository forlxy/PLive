//
// Created by 李金海 on 2019-08-23.
//

#pragma once

#include <list>
#include <string>
#include <utils/macro_util.h>

HODOR_NAMESPACE_START

typedef struct Segment_ {
    std::string url;
    int64_t duration;
    int64_t size;
    int64_t url_offset;
    int     seq_no;
} Segment;

class HlsPlaylistParser {
  public:
    HlsPlaylistParser(std::string manifest_json, int perfer_bandwidth);
    std::list<Segment> getSegmentList();
    std::string getSegmentCacheKey(int seg_seq_no);

  private:
    std::list<Segment> segments_;
    std::string rep_cache_key_;
    int start_seq_no_;
    void parse_playlist(std::string manifest, std::string host_url);
};

HODOR_NAMESPACE_END
