#pragma once

#include <stdint.h>
#include "data_spec.h"
#include "stats.h"
#include "player_statistic.h"

namespace kuaishou {
namespace cache {

class DataSource {
  public:

    virtual ~DataSource() {}
    /**
     * Opens the source to read the specified data.
     * <p>
     * Note: If an {@link IOException} is thrown, callers must still call {@link #close()} to ensure
     * that any partial effects of the invocation are cleaned up.
     *
     * @param dataSpec Defines the data to be read.
     * @throws IOException If an error occurs opening the source. {@link DataSourceException} can be
     *     thrown or used as a cause of the thrown exception to specify the reason of the error.
     * @return The number of bytes that can be read from the opened source. For unbounded requests
     *     (i.e. requests where {@link DataSpec#length} equals {@link C#LENGTH_UNSET}) this value
     *     is the resolved length of the request, or {@link C#LENGTH_UNSET} if the length is still
     *     unresolved. For all other requests, the value returned will be equal to the request's
     *     {@link DataSpec#length}.
     */
    virtual int64_t Open(const DataSpec& spec) = 0;

    /**
     * Reads up to {@code length} bytes of data and stores them into {@code buffer}, starting at
     * index {@code offset}.
     * <p>
     * If {@code length} is zero then 0 is returned. Otherwise, if no data is available because the
     * end of the opened range has been reached, then {@link C#RESULT_END_OF_INPUT} is returned.
     * Otherwise, the call will block until at least one byte of data has been read and the number of
     * bytes read is returned.
     *
     * @param buffer The buffer into which the read data should be stored.
     * @param offset The start offset into {@code buffer} at which data should be written.
     * @param readLength The maximum number of bytes to read.
     * @return The number of bytes read, or {@link C#RESULT_END_OF_INPUT} if no data is available
     *     because the end of the opened range has been reached.
     * @throws IOException If an error occurs reading from the source.
     */
    virtual int64_t Read(uint8_t* buf, int64_t offset, int64_t read_len) = 0;

    /**
     * Closes the source.
     * <p>
     * Note: This method must be called even if the corresponding call to {@link #open(DataSpec)}
     * threw an {@link IOException}. See {@link #open(DataSpec)} for more details.
     *
     * @throws IOException If an error occurs closing the source.
     */
    virtual AcResultType Close() = 0;

    virtual void LimitCurlSpeed() {
        return;
    }

    /**
     * Get Statistics
     * Called after closing the data source to get the stats of last open session.
     * Note, data source can be opened more than one time, this will only return last session.
     */
    virtual Stats* GetStats() = 0;

    void SetContextId(int id) {context_id_ = id;}

    int GetContextId() {return context_id_;}
  private:
    int context_id_ = -1;
};

}
}
