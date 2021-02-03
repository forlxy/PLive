//
//  sliding_percentile.h
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/12/12.
//  Copyright © 2017年 kuaishou. All rights reserved.
//

#pragma once
#include <stdint.h>
#include <vector>

namespace kuaishou {
namespace cache {

/**
 * Default page size bytes.
 */
static const long kMaxRecycledSamples = 5;

typedef enum {
    kSortOrderNone = -1,
    kSortOrderByValue = 0,
    kSortOrderByIndex = 1,
} SortOrder;

struct Sample {
    Sample(int32_t idx = 0, int32_t wei = 0, float val = 0.0) :
        index(idx), weight(wei), value(val) {
    }

    int32_t index = 0;
    int32_t weight = 0;
    float value = 0.0;
};

class SlidingPercentile {
  public:
    /**
     * @param max_weight The maximum weight.
     */
    SlidingPercentile(int32_t max_weight);
    ~SlidingPercentile();

    /**
     * Adds a new weighted value.
     *
     * @param weight The weight of the new observation.
     * @param value The value of the new observation.
     */
    void AddSample(int32_t weight, float value);

    /**
     * Computes a percentile by integration.
     *
     * @param percentile The desired percentile, expressed as a fraction in the range (0,1].
     * @return The requested percentile value or {@link Float#NaN} if no samples have been added.
     */
    float GetPercentile(float percentile);

  private:
    /**
     * Sorts the samples by index.
     */
    void EnsureSortedByIndex();

    /**
     * Sorts the samples by value.
     */
    void EnsureSortedByValue();

  private:
    const int32_t max_weight_;
    std::vector<Sample> samples_;
    int32_t recycled_sample_count_;
    Sample* recycled_samples_;
    SortOrder current_sort_order_;
    int32_t total_weight_;
    int32_t next_sample_index_;
};

} // cache
} // kuaishou
