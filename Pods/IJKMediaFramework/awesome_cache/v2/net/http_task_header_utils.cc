#include "http_task_header_utils.h"

#include <map>
#include <regex>
#include <json/json.h>
#include "utility.h"

using nlohmann::json;


static const int MAX_HTTP_X_KS_COUNT = 10;
static const bool kVerbose = false;

using namespace kuaishou::cache;

void http::SetRequestHeaders(const DataSpec& spec,
                             const DownloadOpts& options,
                             ConnectionInfoV2* info,
                             std::function<void(std::string const&, std::string const&)> add_header_cb) {
    if (!options.headers.empty()) {
        auto headers = kpbase::StringUtil::Split(options.headers, "\r\n");
        for (std::string& header : headers) {
            // 看下是否用了http-dns，如果用了，则放到
            auto key_value_vec = kpbase::StringUtil::Split(header, ": ");
            if (key_value_vec.size() == 2) {
                auto key = kpbase::StringUtil::Trim(key_value_vec[0]);
                auto value = kpbase::StringUtil::Trim(key_value_vec[1]);

                add_header_cb(key, value);

                if (key == "Host" || key == "host") {
                    info->http_dns_host = value;
                }
            }
        }
    }

    if (spec.position != 0 || spec.length > 0) {
        info->is_range_request = true;
        info->range_request_start = spec.position;
        std::string range_request =
            "bytes=" + kpbase::StringUtil::Int2Str(spec.position) + "-";
        if (spec.length != kLengthUnset && spec.length > 0) {
            int64_t range_end = spec.position + spec.length - 1;
            range_request += kpbase::StringUtil::Int2Str(range_end);
            info->range_request_end = range_end;
        }
        add_header_cb("Range", range_request);
    }
}

AcResultType http::ParseResponseHeaders(const DataSpec& spec,
                                        ConnectionInfoV2* info,
                                        bool* server_not_support_range,
                                        int64_t* to_skip_bytes,
                                        std::function<std::pair<std::string, std::string>()> get_next_header) {
    std::map<std::string, std::string> http_headers;
    std::vector<std::string> http_x_ks_headers;
    int http_x_ks_cnt = 0;
    int http_x_ks_str_len = strlen("x-ks-");

    while (true) {
        std::string name, value;
        std::tie(name, value) = get_next_header();
        if (name == "") {
            break;
        }
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name = kpbase::StringUtil::Trim(name);
        value = kpbase::StringUtil::Trim(value);

        if (!name.compare(0, http_x_ks_str_len, "x-ks-")) {
            http_x_ks_cnt++;
            if (http_x_ks_cnt < MAX_HTTP_X_KS_COUNT) {
                std::string x_ks_cache_value = name + ": " + value;
                http_x_ks_headers.push_back(x_ks_cache_value);
            }
        } else {
            http_headers[name] = value;
        }
    }

    if (http_x_ks_cnt == 0) {
        info->x_ks_cache = "{}";
    } else {
        json j;
        for (auto& s : http_x_ks_headers) {
            j.push_back(s);
        }
        info->x_ks_cache = j.dump();
    }


    // content-length strong check
    int64_t content_length = kLengthUnset;
    if (http_headers.find("content-length") != http_headers.end()) {
        auto maybe_length = kpbase::StringUtil::Str2Int(http_headers["content-length"]);
        if (!maybe_length.IsNull()) {
            content_length = maybe_length.Value();
            if (content_length <= 0) {
                LOG_ERROR_DETAIL("ParseResponseHeaders: content_length(%lld) < 0 , error_code = %d",
                                 content_length, kResultExceptionHttpDataSourceInvalidContentLength);
                return kResultExceptionHttpDataSourceInvalidContentLength;
            } else {
                // legal content length
            }
        } else {
            LOG_ERROR_DETAIL("ParseResponseHeaders: content-length has no value , error_code = %d",
                             kResultExceptionHttpDataSourceInvalidContentLength);
            return kResultExceptionHttpDataSourceInvalidContentLength;
        }
    } else {
        // not support allow_content_length_unset for now
        LOG_ERROR_DETAIL("ParseResponseHeaders: content-length not found , error_code = %d",
                         kResultExceptionHttpDataSourceNoContentLength);
        return kResultExceptionHttpDataSourceNoContentLength;
    }
    assert(content_length > 0);
    info->content_length = content_length;

    // range strong check
    if (info->is_range_request) {
        // it's a range request
        std::map<std::string, std::string>::iterator it = http_headers.find("content-range");
        if (it == http_headers.end()) {
            // server not support range request
            info->range_response_file_length = content_length;
            if (spec.position >= content_length) {
                LOG_ERROR_DETAIL("ParseResponseHeaders: expect range response(spec_.position:%lld, "
                                 "spec_.length:%lld) server not support range request, and spec.position(%lld) > content_length(%lld), return %d",
                                 spec.position, spec.length,
                                 spec.position, content_length, kCurlHttpPositionOverflowContentLength);
                return kCurlHttpPositionOverflowContentLength;
            } else {
                LOG_WARN("ParseResponseHeaders: expect range response(spec_.position:%lld, "
                         "spec_.length:%lld) server not support range request, will work with skip bytes:%lld",
                         spec.position, spec.length, spec.position);
                *server_not_support_range = true;
                *to_skip_bytes = spec.position;
            }
        } else {
            // 示例：Content-Range: bytes 0-1048575/6897343
            std::string content_range = it->second;
            std::regex reg1(R"(bytes (\d+)-(\d+)/(\d+))");
            smatch m;
            regex_match(content_range, m, reg1);
            info->range_response_start = m[1].matched ? kpbase::StringUtil::Str2Int(m[1].str()).Value() : -1;
            info->range_response_end = m[2].matched ? kpbase::StringUtil::Str2Int(m[2].str()).Value() : -1;
            info->range_response_file_length = m[3].matched ? kpbase::StringUtil::Str2Int(m[3].str()).Value() : -1;

            if (info->range_response_start != info->range_request_start
                || info->range_response_file_length < 0) {
                // 目前暂时要求range end也是严格符合要求的，后续可以考虑 response_end >= range_request_end即可
                LOG_ERROR_DETAIL("ParseResponseHeaders: range response not valid, "
                                 "expect %lld ~ %lld, response:%lld ~ %lld, return error:%d",
                                 info->range_request_start, info->range_request_end,
                                 info->range_response_start, info->range_response_end,
                                 kCurlHttpResponseRangeInvalid);
                return kCurlHttpResponseRangeInvalid;
            } else if (kVerbose) {
                LOG_INFO("ParseResponseHeaders: range response parse success, expect %lld ~ %lld, response:%lld ~ %lld(%lld)",
                         info->range_request_start, info->range_request_end,
                         info->range_response_start, info->range_response_end,
                         info->range_response_file_length);
            }
        }
    }

    // response_code weak check
    if (http_headers.find("kwaisign") != http_headers.end()) {
        info->sign = http_headers["kwaisign"];
    } else {
        info->sign = "no value";
    }

    // gzip weak check
    if (http_headers.find("content-encoding") != http_headers.end()) {
        if (kpbase::StringUtil::Trim(http_headers["content-encoding"]) == "gzip") {
            info->is_gzip = true;
        }
    }

    return kResultOK;
}
