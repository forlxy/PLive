//
// Created by MarshallShuai on 2018/11/3.
//

#include "dcc_algorithm_c.h"
#include <algorithm>
#include "ac_log.h"
#include <include/dcc_algorithm_c.h>
#include "utility.h"

static const char* TAG = "DccAlgorithm";
static const int kVerbose = false;

static const int kMinPreReadDurMs = 100;
static const int kMaxPreReadDurMs = 2 * 60 * 1000;
static const int kDefaultPreReadDurMsForGoodNetMs = 5 * 1000;

static const int kExpirePeriedMs = 5 * 60 * 1000; // 正常过期应该设置大于5分钟级别的


class MarkRecord {
  public:
    MarkRecord() : is_valid_(false),
        net_type_(Net_WIFI), // fixme 后续改成 Net_UNKNOWN
        update_ts_ms_(0),
        mark_kbps_(0),
        expire_period_ms_(kExpirePeriedMs) {}

    bool IsValid(std::string& invalid_reason) {
        bool valid = false;

        if (!is_valid_) {
            invalid_reason = "首次播放，无网速信息可参考";
        } else if (net_type_ != Net_WIFI && net_type_ != Net_4G) {
            invalid_reason = "网络不符合要求，当前网络:" + GetNetTypeStr();
        } else if (mark_kbps_ <= 0) {
            invalid_reason = "最后一次记录网速 mark_kbps(" + std::to_string(mark_kbps_) + ") <= 0";
        } else if (update_ts_ms_ <= 0) {
            invalid_reason = "最后一次记录时间戳 update_ts_ms_(" + std::to_string(update_ts_ms_) + ") <= 0";
        } else if (IsExpired()) {
            invalid_reason = "最后一次记录已过期（超过" + std::to_string(expire_period_ms_ / 1000) + "秒)";
        } else {
            valid = true;
        }


        if (!valid) {
            LOG_WARN("[%s][MarkRecord valid:%d] is_valid = %d, net_type:%d ,mark_kbps_:%d",
                     TAG, valid, is_valid_, net_type_, mark_kbps_);
        } else if (kVerbose) {
            LOG_INFO("[%s][MarkRecord valid:%d] is_valid = %d, net_type:%d ,mark_kbps_:%d",
                     TAG, valid, is_valid_, net_type_, mark_kbps_);
        }
        return valid;
    }

    std::string GetNetTypeStr() {
        switch (net_type_) {
            case Net_WIFI: return "WIFI";
            case Net_4G: return "4G";
            case Net_OTHERS: return "OTHER";
            case Net_UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    bool IsExpired() {
        int64_t now = kuaishou::kpbase::SystemUtil::GetCPUTime();
        int64_t diff_ms = now - update_ts_ms_;
        bool expired = diff_ms > expire_period_ms_;
        if (expired) {
            LOG_WARN("[%s][%s], expired:%d, diff_ms:%d,  expirePeriedMs:%d", TAG, __func__,
                     expired, (int) diff_ms, expire_period_ms_);
        } else if (kVerbose) {
            LOG_INFO("[%s][%s], expired:%d, diff_ms:%d,  expirePeriedMs:%d", TAG, __func__,
                     expired, (int) diff_ms, expire_period_ms_);
        }
        return expired;
    }

    void UpdateMarkKbps(int mark_kbps) {
        is_valid_ = true;
        mark_kbps_ = mark_kbps;
        update_ts_ms_ = kuaishou::kpbase::SystemUtil::GetCPUTime();
        if (kVerbose) {
            LOG_INFO("[%s][%s], mark_kbps:%d", TAG, __func__, mark_kbps);
        }
    }

    bool is_valid_;
    DccAlgorithm_NetworkType net_type_;

    int64_t update_ts_ms_;
    int mark_kbps_;
    int expire_period_ms_;

};

static std::mutex record_mutex;
static MarkRecord s_last_mark_record;

void DccAlgorithm_update_speed_mark(int mark_kbps) {
    if (mark_kbps <= 0) {
        LOG_ERROR("[DccAlgorithm_update_speed_mark] mark_kbps:%d, invalid", mark_kbps);
        return;
    }
    std::lock_guard<std::mutex> lg(record_mutex);
    s_last_mark_record.UpdateMarkKbps(mark_kbps);
}

int DccAlgorithm_get_current_speed_mark() {
    std::lock_guard<std::mutex> lg(record_mutex);
    return s_last_mark_record.mark_kbps_;
}


void DccAlgorithm_onNetworkChange(DccAlgorithm_NetworkType net_type) {
    std::lock_guard<std::mutex> lg(record_mutex);
    s_last_mark_record.is_valid_ = false;
    s_last_mark_record.net_type_ = net_type;


    LOG_INFO("[%s], net_type:%d", __func__, net_type);
}

void DccAlgorithm_init(DccAlgorithm* alg) {
    alg->config_dcc_pre_read_ms = kDefaultPreReadDurMsForGoodNetMs;
    alg->config_enabled = 0;
    alg->config_mark_bitrate_th_10 = 3 * 10;

    alg->qos_used = false;
    alg->qos_dcc_pre_read_ms_used = -1;
    alg->qos_dcc_actual_mb_ratio = -1.0f;
}


static inline const char* DccAlgorithm_is_enable_status(DccAlgorithm* alg) {
    return alg->config_enabled ? "开启" : "未开启";
}

int DccAlgorithm_get_pre_read_duration_ms(DccAlgorithm* alg,
                                          int default_ret,
                                          int64_t meta_dur_ms,
                                          int64_t meta_bitrate_kbps) {
    std::lock_guard<std::mutex> lg(record_mutex);
    int pre_read_ms_ret = default_ret;
    std::string not_valid_reason;
    if (!s_last_mark_record.IsValid(not_valid_reason)) {
        snprintf(alg->status, DCC_ALG_STATUS_MAX_LEN, "%s | th:%d/pre_read:%dms | 未使用 | 原因：%s",
                 DccAlgorithm_is_enable_status(alg),
                 alg->config_mark_bitrate_th_10, alg->config_dcc_pre_read_ms,
                 not_valid_reason.c_str());
        return default_ret;
    }

    if (meta_dur_ms <= 0 || meta_bitrate_kbps <= 0) {
        LOG_WARN("[%s], meta param not ok, meta_dur_ms = %d, meta_bitrate_kbps:%d ,return",
                 __func__, meta_dur_ms, meta_bitrate_kbps);
        snprintf(alg->status, DCC_ALG_STATUS_MAX_LEN,
                 "%s | th:%d/pre_read:%dms | 未使用 | 原因：meta_dur_ms（%lld) |meta_dur_ms(%lld) is invalid",
                 DccAlgorithm_is_enable_status(alg),
                 alg->config_mark_bitrate_th_10,
                 alg->config_dcc_pre_read_ms,
                 meta_dur_ms, meta_bitrate_kbps);
        return default_ret;
    }

    // 算法部分
    alg->cmp_mark_kbps = s_last_mark_record.mark_kbps_;
    float mb_ratio = 1.0f * s_last_mark_record.mark_kbps_ / meta_bitrate_kbps;
    float mb_th = 1.0f * alg->config_mark_bitrate_th_10 / 10;
    if (mb_ratio > mb_th) {
        alg->qos_used = true;
        snprintf(alg->status, DCC_ALG_STATUS_MAX_LEN, "%s | th:%d/pre_read:%dms |使用 | mb_ratio:%4.2f > mb_th:%4.2f",
                 DccAlgorithm_is_enable_status(alg),
                 alg->config_mark_bitrate_th_10,
                 alg->config_dcc_pre_read_ms,
                 mb_ratio, mb_th);
        pre_read_ms_ret = alg->config_dcc_pre_read_ms;
    } else {
        snprintf(alg->status, DCC_ALG_STATUS_MAX_LEN, "%s | th:%d/pre_read:%dms |未使用 | mb_ratio:%4.2f < mb_th:%4.2f",
                 DccAlgorithm_is_enable_status(alg),
                 alg->config_mark_bitrate_th_10,
                 alg->config_dcc_pre_read_ms, mb_ratio, mb_th);
        pre_read_ms_ret = default_ret;
        goto RETURN_LABEL;
    }

RETURN_LABEL:

    // 最后一道防护
    pre_read_ms_ret = std::min(std::max(kMinPreReadDurMs, pre_read_ms_ret), kMaxPreReadDurMs);
    if (kVerbose) {
        LOG_DEBUG(
            "[%s], pre_read_ms_ret:%d, meta_dur_ms:%lld, meta_bitrate_kbps:%lld, mb_ratio:%f, mb_th:%f",
            __func__, pre_read_ms_ret, meta_dur_ms, meta_bitrate_kbps, mb_ratio, mb_th);
    }
    alg->qos_dcc_pre_read_ms_used = pre_read_ms_ret;
    alg->qos_dcc_actual_mb_ratio = mb_ratio;

    return pre_read_ms_ret;
}
