#pragma once

#include <stdint.h>
#include <string>
#include "constant.h"

namespace kuaishou {
namespace cache {

struct DataSpec {
    DataSpec() {}

    std::string uri = "";

    // use string as the container of post body
    std::string body = "";

    std::string key = "";

    /**
     * The absolute position of the data in the full stream.
     */
    int64_t absolute_stream_position = 0;
    /**
    * The position of the data when read from {@link #uri}.
    * <p>
    * Always equal to {@link #absoluteStreamPosition} unless the {@link #uri} defines the location
    * of a subset of the underlying data.
    */
    int64_t position = 0;

    // the length of content to read from uri
    int64_t length = kLengthUnset;

    // http request specific

    // http request timeout, unit ms.
    int32_t timeout = kTimeoutUnSet;

    /**
     * Request flags. Currently {@link #FLAG_ALLOW_GZIP} and
     * {@link #FLAG_ALLOW_CACHING_UNKNOWN_LENGTH} are the only supported flags.
     */
    int32_t flags = 0;

    DataSpec& WithUri(const std::string& _uri) {
        this->uri = _uri;
        return *this;
    }

    DataSpec& WithBody(const std::string& _body) {
        this->body = _body;
        return *this;
    }

    DataSpec& WithPosition(int64_t _position) {
        this->absolute_stream_position = _position;
        this->position = _position;
        return *this;
    }

    DataSpec& WithPositions(int64_t _abs_position, int64_t _position) {
        this->absolute_stream_position = _abs_position;
        this->position = _position;
        return *this;
    }

    DataSpec& WithLength(int64_t _length) {
        this->length = _length;
        return *this;
    }

    DataSpec& WithTimeout(int32_t _timeout) {
        this->timeout = _timeout;
        return *this;
    }

    DataSpec& WithFlags(int32_t _flags) {
        this->flags = _flags;
        return *this;
    }

    DataSpec& WithKey(const std::string& _key) {
        this->key = _key;
        return *this;
    }
};

}
}
