//
// Created by MarshallShuai on 2019-08-15.
//

#include "hodor_c.h"
#include <sstream>
#include "utils/macro_util.h"
#include "hodor_downloader/hodor_downloader.h"

USING_HODOR_NAMESPAE

const char* Hodor_get_status_for_debug_info(char* buf, int buf_len) {
    HodorDownloader::GetInstance()->GetStatusForDebugInfo(buf, buf_len);
    return buf;
}
