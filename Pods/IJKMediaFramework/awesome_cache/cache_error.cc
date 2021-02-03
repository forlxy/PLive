//
// Created by 帅龙成 on 2018/11/4.
//

#include <curl/curl.h>
#include "cache_errors.h"


#define ERROR_MSG(ID, NAME, ERR_MSG, ERR_DESC) case ID: return ERR_MSG;
#define ERROR_DESC(ID, NAME, ERR_MSG, ERR_DESC) case ID: return ERR_DESC;

#define BETWEEN(x, a, b) x > a && x < b
#define IS_HTTP_RESPONCE_CODE_ERROR(x) BETWEEN(x, kLibcurlErrorBase, kHttpInvalidResponseCodeBase)
#define IS_CURL_ERROR(x) BETWEEN(x, kNsUrlErrorBase, kLibcurlErrorBase)
// deprecated，暂停维护
#define IS_IOS_DOWNLOAD_TASK_ERROR(x) BETWEEN(x, kCacheErrorCodeMin, kNsUrlErrorBase)

bool is_cache_error(int error) {
    return error > kCacheErrorCodeMin && error < kCacheErrorCodeMax;
}

bool is_cache_abort_by_callback_error_code(int error) {
    return error == kLibcurlErrorBase + (-CURLE_ABORTED_BY_CALLBACK);
}

const char* cache_error_msg(int code) {
    if (IS_HTTP_RESPONCE_CODE_ERROR(code)) {
        return "HttpResponseCodeError";
    } else if (IS_CURL_ERROR(code)) {
#if defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_OS_MAC
        return "LibCurl NetworkError";
#else
        CURLcode curlErr = (CURLcode)(kLibcurlErrorBase - code);
        return curl_easy_strerror(curlErr);
#endif
#else
        CURLcode curlErr = (CURLcode)(kLibcurlErrorBase - code);
        return curl_easy_strerror(curlErr);
#endif
    }

    switch (code) {
            AWESOME_CACHE_ERROR_CODES(ERROR_MSG, DIVIDER_AND_COMMENT)
            return "Unknown AwesomeCache error";
    }

    return "Unknown AwesomeCache error";
}

const char* cache_error_desc(int code) {
    if (IS_HTTP_RESPONCE_CODE_ERROR(code)) {
        return "HttpResponseCodeError";
    } else if (IS_CURL_ERROR(code)) {
        return "LibCurlError";
    }

    switch (code) {
            AWESOME_CACHE_ERROR_CODES(ERROR_DESC, DIVIDER_AND_COMMENT)
        default:
            return "Unknown AwesomeCache Desc";
    }

    return "Unknown AwesomeCache Desc";
}


#undef BETWEEN
#undef ERROR_MSG
#undef ERROR_DESC

