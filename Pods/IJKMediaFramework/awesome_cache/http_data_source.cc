#include "http_data_source.h"

namespace kuaishou {
namespace cache {

HttpDataSourceStats::HttpDataSourceStats() : DefaultDataStats("HttpDataSource") {
}

void HttpDataSourceStats::FillJson() {
    DefaultDataStats::FillJson();
    stats_["connect_time"] = connect_time_ms;
    stats_["response_code"] = response_code;
}

void HttpDataSource::SetRequestProperties(const std::string& name, const std::string& value) {
    request_property_map_[name] = value;
}

void HttpDataSource::ClearRequestProperty(const std::string& name) {
    auto it = request_property_map_.find(name);
    if (it != request_property_map_.end()) {
        request_property_map_.erase(it);
    }
}

void HttpDataSource::ClearAllRequestProperties() {
    request_property_map_.clear();
}

bool HttpDataSource::HasRequestProperty(const std::string& name) {
    return request_property_map_.find(name) != request_property_map_.end();
}

std::string HttpDataSource::GetRequestProperty(const std::string& name) {
    return HasRequestProperty(name) ? request_property_map_[name] : "";
}

}
}
