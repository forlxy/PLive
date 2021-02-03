//
// Created by 帅龙成 on 19/10/2017.
//
#pragma once

#include "data_source.h"

namespace kuaishou {
namespace cache {

template<typename S>
class TransferListener {
  public:
    /**
    * Called when a transfer starts.
    *
    * @param source The source performing the transfer.
    * @param spec Describes the data being transferred.
    */
    virtual void OnTransferStart(S* source, const DataSpec& spec) = 0;

    /**
     * Called incrementally during a transfer.
     *
     * @param source The source performing the transfer.
     * @param byte_transfered The number of bytes transferred since the previous call to this
     *     method (or if the first call, since the transfer was started).
     */
    virtual void OnBytesTransfered(S* source, int64_t byte_transfered) = 0;

    /**
     * Called when a transfer ends.
     *
     * @param source The source performing the transfer.
     */
    virtual void OnTransferEnd(S* source) = 0;
};

}
}
