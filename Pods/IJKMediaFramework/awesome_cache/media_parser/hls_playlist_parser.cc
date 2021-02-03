#include "hls_playlist_parser.h"
#include "media/kwai_hls_manifest_parser_c.h"

#include <sstream>

using namespace std;

HODOR_NAMESPACE_START

#define AV_TIME_BASE            1000000

HlsPlaylistParser::HlsPlaylistParser(std::string manifest_json, int perfer_bandwidth) {
    AdaptationSet* adaptationSet = AdaptationSet_create();
    const char* json = manifest_json.c_str();
    int ret = AdaptationSet_parse_hls_manifest_json(json, adaptationSet);
    if (ret > 0) {
        HlsRepresentation* rep = select_prefer_rep(adaptationSet, perfer_bandwidth);
        if (rep) {
            rep_cache_key_ = rep->cache_key ? rep->cache_key : "";
            parse_playlist(rep->manifest_content && strlen(rep->manifest_content) ? rep->manifest_content :
                           (rep->manifest_slice && strlen(rep->manifest_slice) ? rep->manifest_slice : ""), rep->base_url ? rep->base_url : (rep->url ? rep->url : ""));
        }
    } else {

    }
}

#define MAX_URL_SIZE 4096

string make_absolute_url(string url, string host_url) {
    if (host_url.empty() || url.find("://") != string::npos) {
        return url;
    }
    string ret(MAX_URL_SIZE, 0);
    ret.replace(0, host_url.length(), host_url);
    size_t pos;
    if (!host_url.empty() && !url.empty() && url[0] == '/') {
        if ((pos = ret.find("://")) != string::npos) {
            if (url[1] == '/') {
                pos += 1;
            } else {
                pos += 3;
                pos = ret.find('/', pos + 1);
            }
        }
        ret.replace(pos, string::npos, url);
        return ret;
    }
    if (url[0] == '?') {
        ret.replace(0, host_url.length(), host_url);
        pos = ret.find('?');
        ret.replace(pos, string::npos, url);
        return ret;
    }
    if ((pos = ret.find_last_of('/')) == string::npos) {
        pos = -1;
    }
    size_t url_pos = 0;
    while (url.find("../", url_pos) == url_pos) {
        pos = ret.find_last_of('/', pos - 1);
        if (pos != string::npos) {
            if (ret.find("..", pos + 1) != string::npos) {
                break;
            }
        } else {
            if (ret.find("..") == 0) {
                break;
            } else {
                pos = -1;
            }
        }
        url_pos += 3;
    }
    ret.replace(pos + 1, string::npos, url.data() + url_pos);
    return ret;
}

void HlsPlaylistParser::parse_playlist(std::string manifest, string host_url) {
    const static string TAG_INF = "#EXTINF:";
    const static string TAG_BYTERANGE = "#EXT-X-BYTERANGE:";
    const static string TAG_MEDIA_SEQUENCE = "#EXTINF:";
    const static string TAG_EXT = "#";
    std::stringstream ss;
    ss.str(manifest);
    string line;
    const char* ptr = NULL;
    int is_segment = 0;
    int64_t duration = 0;
    int64_t seg_offset = 0;
    int64_t seg_size = -1;
    int start_seq_no = 0;
    int seg_index = 0;
    string next_str;
    int pos = 0;
    while (getline(ss, line)) {
        if ((pos = (int)line.find(TAG_INF)) == 0) {
            ptr = line.c_str() + TAG_INF.length();
            is_segment = 1;
            duration   = atof(ptr) * AV_TIME_BASE;
        } else if ((pos = (int)line.find(TAG_BYTERANGE)) == 0) {
            next_str = line.substr(pos + TAG_BYTERANGE.length());
            ptr = line.c_str() + TAG_BYTERANGE.length();
            seg_size = strtoll(ptr, NULL, 10);
            ptr = strchr(ptr, '@');
            if (ptr) {
                seg_offset = strtoll(ptr + 1, NULL, 10);
            }
        } else if ((pos = (int)line.find(TAG_MEDIA_SEQUENCE)) == 0) {
            next_str = line.substr(pos + TAG_MEDIA_SEQUENCE.length());
            ptr = line.c_str() + TAG_MEDIA_SEQUENCE.length();
            start_seq_no = atoi(ptr);
        } else if (line.find(TAG_EXT) == 0) {
            continue;
        } else if (line[0]) {
            if (is_segment) {
                Segment seg;
                seg.duration = duration;
                seg.url = make_absolute_url(line, host_url);
                is_segment = 0;
                seg.size = seg_size;
                seg.seq_no = start_seq_no + seg_index;
                if (seg_size >= 0) {
                    seg.url_offset = seg_offset;
                    seg_offset += seg_size;
                    seg_size = -1;
                } else {
                    seg.url_offset = 0;
                    seg_offset = 0;
                }
                segments_.push_back(seg);
                ++seg_index;
            }
        }
    }
}

list<Segment> HlsPlaylistParser::getSegmentList() {
    return segments_;
}

std::string HlsPlaylistParser::getSegmentCacheKey(int seg_seq_no) {
    return rep_cache_key_ + "_" +  to_string(seg_seq_no + 1);
}

HODOR_NAMESPACE_END

