//
//  bandwidth_meter.h
//  IJKMediaPlayer
//
//  Created by wangtao03 on 2017/12/12.
//  Copyright © 2017年 kuaishou. All rights reserved.
//
#pragma once
#include <stdint.h>


namespace kuaishou {
namespace cache {

class BandwidthMeter {
  public:
    class EventListener {
      public:
        virtual ~EventListener() {};
        /**
         * Called periodically to indicate that bytes have been transferred.
         * <p>
         * Note: The estimated bitrate is typically derived from more information than just
         * {@code bytes} and {@code elapsedMs}.
         *
         * @param elapsedMs The time taken to transfer the bytes, in milliseconds.
         * @param bytes The number of bytes transferred.
         * @param bitrate The estimated bitrate in bits/sec, or {@link #NO_ESTIMATE} if an estimate is
         *     not available.
         */
        virtual void OnBandwidthSample(int32_t elapsed_ms, int32_t bytes, int32_t bitrate) = 0;
    };

    virtual int32_t GetBitrateEstimate() = 0;

};

} // cache
} // kuaishou
