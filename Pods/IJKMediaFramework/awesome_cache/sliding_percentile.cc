//
//  sliding_percentile.cc
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/12/12.
//  Copyright © 2017年 kuaishou. All rights reserved.
//

#include "sliding_percentile.h"
#include <algorithm>

namespace kuaishou {
namespace cache {

struct SampleIndexComparator {
    bool operator()(const Sample& a, const Sample& b) {
        return a.index < b.index;
    }
} index_comparator;

struct SampleValueComparator {
    bool operator()(const Sample& a, const Sample& b) {
        return a.value < b.value ? true : (b.value < a.value ? false : true);
    }
} value_comparator;

SlidingPercentile::SlidingPercentile(int32_t maxWeight) :
    max_weight_(maxWeight),
    recycled_samples_(new Sample[kMaxRecycledSamples]),
    current_sort_order_(kSortOrderNone),
    recycled_sample_count_(0) {
    // Fix me
}

SlidingPercentile::~SlidingPercentile() {
    if (recycled_samples_) {
        delete [] recycled_samples_;
        recycled_samples_ = nullptr;
    }
}

void SlidingPercentile::AddSample(int32_t weight, float value) {
    EnsureSortedByIndex();

    Sample new_sample = (recycled_sample_count_ > 0) ? (recycled_samples_[--recycled_sample_count_])
                        : Sample();
    new_sample.index = next_sample_index_++;
    new_sample.weight = weight;
    new_sample.value = value;
    samples_.push_back(new_sample);
    total_weight_ += weight;

    while (total_weight_ > max_weight_) {
        int32_t excess_weight = total_weight_ - max_weight_;
        Sample oldest_sample = samples_[0];
        if (oldest_sample.weight <= excess_weight) {
            total_weight_ -= oldest_sample.weight;
            samples_.erase(samples_.begin());
            if (recycled_sample_count_ < kMaxRecycledSamples) {
                recycled_samples_[recycled_sample_count_++] = oldest_sample;
            }
        } else {
            oldest_sample.weight -= excess_weight;
            total_weight_ -= excess_weight;
        }
    }
}

float SlidingPercentile::GetPercentile(float percentile) {
    EnsureSortedByValue();
    float desired_weight = percentile * total_weight_;
    int32_t accumulated_weight = 0;
    for (int i = 0; i < samples_.size(); i++) {
        Sample current_sample = samples_[0];
        accumulated_weight += current_sample.weight;
        if (accumulated_weight >= desired_weight) {
            return current_sample.value;
        }
    }

    return samples_.empty() ? 0.0 : samples_[samples_.size() - 1].value;
}

void SlidingPercentile::EnsureSortedByIndex() {
    if (!samples_.empty() && current_sort_order_ != kSortOrderByIndex) {
        std::sort(samples_.begin(), samples_.end(), index_comparator);
        current_sort_order_ = kSortOrderByIndex;
    }
}

void SlidingPercentile::EnsureSortedByValue() {
    if (!samples_.empty() && current_sort_order_ != kSortOrderByValue) {
        std::sort(samples_.begin(), samples_.end(), value_comparator);
        current_sort_order_ = kSortOrderByValue;
    }
}

} // cache
} // kuaishou
