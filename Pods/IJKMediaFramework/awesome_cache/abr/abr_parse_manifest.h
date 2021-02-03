#pragma once

#include <cstdint>
#include <string>
#include <libavkwai/cJSON.h>
#include "abr_types.h"

#define MAX_URL_SIZE  4096
#define MAX_REPRSENTATION_COUNT  16
#define UNKNOWN_DOWNLOAD_LEN     0   // can't get download len from manifest

namespace kuaishou {
namespace abr {
class AbrParseManifest {
  public:
    AbrParseManifest();
    virtual ~AbrParseManifest() {}
    int AbrEngienAdaptInit();
    void Init(uint32_t dev_res_width, uint32_t dev_res_heigh,
              uint32_t net_type, uint32_t low_device,
              uint32_t signal_strength, uint32_t switch_code);
    int ParserVodAdaptiveManifest(const std::string& url);
    int ParserRateConfig(const std::string& rate_config);
    RateAdaptConfig& GetConfigRate();
    AdaptionProfile& GetAdaptionProfile();
    int GetCachedIndex();
    char* GetUrl(int index);
    char* GetHost(int index);
    char* GetKey(int index);
    int GetDownloadLen(int index);
    int GetAvgBitrate(int index);

  private:
    int GetItemValueInt(cJSON* root, const char* name, int default_value);
    double GetItemValueDouble(cJSON* root, const char* name, double default_value);
    int GetItemValueStr(cJSON* root, const char* name, char* dst);
    void CopyToAdaptProfiles();
    void InitRateConfig();

    RateAdaptConfig rate_config_;
    AdaptionProfile adaption_profile_;

    typedef struct VideoResolution {
        uint32_t height;
        uint32_t width;
    } VideoResolution;

    typedef struct DeviceResolution {
        uint32_t height;
        uint32_t width;
    } DeviceResolution;

    typedef struct Representation {
        char url[MAX_URL_SIZE + 1];
        char host[MAX_URL_SIZE + 1];
        char key[MAX_URL_SIZE + 1];
        uint32_t representation_id;     // unique id used to identify each video profile
        uint32_t max_bitrate_kbps;      // max bitrate in kbps
        uint32_t avg_bitrate_kbps;      // average bitrate in kbps
        float quality;
        VideoResolution video_resolution;

        int32_t download_len;
    } Representation;

    typedef struct VodPlayList {
        // param from manifest
        int adaptation_id;
        int duration;
        int rep_count;
        Representation rep[MAX_REPRSENTATION_COUNT];
        // param from app
        enum NetworkType net_type;
        DeviceResolution device_resolution;
        int cached;
        int32_t low_device;
        int32_t signal_strength;
        int32_t switch_code;
    } VodPlayList;

    VodPlayList vod_playlist_;
};
}
}
