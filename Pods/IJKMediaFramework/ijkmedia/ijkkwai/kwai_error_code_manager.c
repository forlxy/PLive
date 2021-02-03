//
// Created by MarshallShuai on 2018/11/5.
//

#include "kwai_error_code_manager.h"
#include "kwai_error_code_manager_ff_convert.h"

#define KWAI_PLAYER_ERROR_MSG(ID, NAME, ERR_MSG, ERR_DESC) case ID: return ERR_MSG;
static const char* get_kwai_player_error_msg(int error) {
    switch (error) {
            KWAI_PLAYER_ERROR_CODES(KWAI_PLAYER_ERROR_MSG);
        default:
            return "Unknown KwaiPlayerErrorCode";
    }
}
#undef KWAI_PLAYER_ERROR_MSG


#if defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_OS_MAC
#define IS_ON_MAC
#endif
#endif

const char* kwai_error_code_to_string(int error) {
    if (error == 0) {
        return "";
    }

    const char* ret = "N/A";

    if (error < EIJK_UNKNOWN_ERROR_BASE) {
        int origin_error = error - EIJK_UNKNOWN_ERROR_BASE;
        if (is_cache_error(origin_error)) {
            return cache_error_msg(origin_error);
        } else {
            return "Unknow Player Error";
        }
    } else {
        return get_kwai_player_error_msg(error);
    }
    return ret;
}


