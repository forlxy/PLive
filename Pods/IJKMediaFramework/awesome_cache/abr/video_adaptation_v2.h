#pragma once
#include <string>
#include <libavkwai/cJSON.h>
#include "video_adaptation_algorithm_interface.h"

namespace kuaishou {
namespace abr {
#define ABSOLUTE_LOW_DEVICE_LOW_RES 1
#define NET_AWARE_LOW_DEVICE_LOW_RES 2

class VideoAdaptationV2 : public VideoAdaptationAlgorithmInterface {
  public:
    VideoAdaptationV2(const RateAdaptConfig& rate_adapt_config, const RateAdaptConfigA1& rate_adapt_config_a1);
    virtual ~VideoAdaptationV2() {}

    uint32_t AdaptPendingNextProfileRepId(BandwidthEstimationAlgorithmInterface* bandwidth_estimate,
                                          uint32_t duration_ms,
                                          AdaptionProfile profile) override;
    VideoAdaptationAlgoType GetVideoAdaptationAlgoType() const override;

    uint32_t AdaptInit(AdaptionProfile& profile);
    uint32_t AdaptInitA1(AdaptionProfile& profile);
    uint32_t AdaptFourG(AdaptionProfile& profile);
    uint32_t AdaptWIFI(AdaptionProfile& profile);
    uint32_t AdaptOtherNet(AdaptionProfile& profile);
    uint32_t AbsoluteLowRate(AdaptionProfile& profile);
    uint32_t AbsoluteLowRes(AdaptionProfile& profile);
    uint32_t AbsoluteHighRes(AdaptionProfile& profile);
    uint32_t AbsoluteMaxbitrate(AdaptionProfile& profile);
    uint32_t DynamicAdapt(AdaptionProfile& profile);
    uint32_t AdaptA1(AdaptionProfile& profile);
    std::vector<kuaishou::abr::VideoProfile>::iterator ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it);
    std::vector<kuaishou::abr::VideoProfile>::iterator ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate);
    std::vector<kuaishou::abr::VideoProfile>::iterator ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, float target_quality);
    std::vector<kuaishou::abr::VideoProfile>::iterator ResolutionPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate, float target_quality);
    std::vector<kuaishou::abr::VideoProfile>::iterator BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it);
    std::vector<kuaishou::abr::VideoProfile>::iterator BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, VideoResolution target_res);
    std::vector<kuaishou::abr::VideoProfile>::iterator BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, float target_quality);
    std::vector<kuaishou::abr::VideoProfile>::iterator BitratePriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, VideoResolution target_res, float target_quality);
    std::vector<kuaishou::abr::VideoProfile>::iterator QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it);
    std::vector<kuaishou::abr::VideoProfile>::iterator QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate);
    std::vector<kuaishou::abr::VideoProfile>::iterator QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, VideoResolution target_res);
    std::vector<kuaishou::abr::VideoProfile>::iterator QualityPriority(AdaptionProfile& profile, std::vector<kuaishou::abr::VideoProfile>::iterator it, uint32_t target_bitrate, VideoResolution target_res);

    uint32_t BandwidthAmend(AdaptionProfile& profile);
    std::string GetSwitchReason() override;
    const char* GetDetailSwitchReason() override;
    void CreateSwitchReason(const char* fmt, ...);
    std::string current_condition() {return current_condition_;};
    std::string bandwidth_computation_process() {return bandwidth_computation_process_;};
    void UpdateConfig(RateAdaptConfig& rate_config, RateAdaptConfigA1& rate_config_a1) override;
    void UpdateHisState(NetworkType net_type, std::vector<kuaishou::abr::VideoProfile>::iterator video_it);
    void SetHistoryData(const std::string& history_data) override;
    std::string GetHistoryData() override;
    uint32_t GetManualAutoState() override;

  private:
    void DumpHistoryData(HisState& his_state, std::string log_tag);
    bool is_init_;
    bool is_avg_bandwidth_zero_;
    RateAdaptConfig rate_adapt_config_;
    RateAdaptConfigA1 rate_adapt_config_a1_;
    HisState his_state_;
    HisState wifi_his_state_;
    HisState foureG_his_state_;
    HisState threeG_his_state_;
    HisState other_his_state_;
    std::string switch_reason_;
    std::string current_condition_;
    std::string bandwidth_computation_process_;
    int32_t avg_bandwidth_;
    int32_t avg_bandwidth_computer_type;
    std::vector<kuaishou::abr::VideoProfile>::iterator target_video_it_;
    BandwidthEstimationAlgorithmInterface* bandwidth_estimate_;
#define DETAIL_SWITCH_REASON_MAX_LEN    256
    char detail_switch_reason_[DETAIL_SWITCH_REASON_MAX_LEN + 1];
    uint32_t manual_auto_state_;
};

}
}
