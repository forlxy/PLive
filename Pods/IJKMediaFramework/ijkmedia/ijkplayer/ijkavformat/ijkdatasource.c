#include <assert.h>
#include "cache_statistic.h"
#include "libavformat/avformat.h"
#include "libavformat/url.h"
#include "libavutil/avstring.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/dict.h"

#include "ijksdl/ijksdl.h"
#include "ijkplayer/ijkavutil/opt.h"
#include "ffmpeg_adapter.h"
#include "awesome_cache_c.h"
#include "ff_ffplay.h"

typedef struct Context {
    AVClass*        class;

    int64_t         opaque;
    int64_t         total_size;
    int64_t         position;
    int64_t         start_offset;
    int64_t         end_offset;
    char*           url;
    char*           user_agent;
    char*           http_proxy;
    char*           headers;
    char*           cookies;
    char*           host_url;
    char*           host_cache_key;
    int             enable_cache;
    int             seg_index;
    FfmpegAdapterQos*   adapter_qos;
    ac_data_source_t  data_source;
} Context;

#define PROTO_NAME "httpdatasource"
static bool kVerbose = true;

char* ijkds_get_hook_url_proto_name() {
    return PROTO_NAME;
}

static char* create_segment_cache_key(char* host_key, int seg_index) {
    if (host_key) {
        return av_asprintf("%s_%d", host_key, seg_index);
    } else {
        return NULL;
    }
}

static int ijkdatasource_close(URLContext* h) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGD("[%s:%d] url:%s", __func__, __LINE__, c->url);
    ac_data_source_close(c->data_source, true);
    ac_data_source_releasep(&c->data_source);
    if (c->seg_index > 0) {
        char* last_segment_cache_key = create_segment_cache_key(c->host_cache_key, c->seg_index - 1);
        hodor_clear_cache_by_key(NULL, last_segment_cache_key);
        av_free(last_segment_cache_key);
    }
    av_freep(&c->url);
    ac_free_strp(&c->host_cache_key);
    return 0;
}

static int ijkdatasource_open(URLContext* h, const char* filename, int flags, AVDictionary** options) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGD("[%s:%d] filename:%s, flags:%d start_offset:%lld end_offset:%lld\n", __func__, __LINE__, filename, flags, c->start_offset, c->end_offset);

    AVIOInterruptCB callback = h->interrupt_callback;
    FFPlayer* ffp = (FFPlayer*)callback.opaque;

    if (callback.callback(ffp)) {
        return 0;
    }

    assert(ffp);
    c->adapter_qos = &ffp->cache_stat.ffmpeg_adapter_qos;

    if (strlen(filename) <= strlen(PROTO_NAME) + 1 || strncmp(PROTO_NAME, filename, strlen(PROTO_NAME)))
        return AVERROR_EXTERNAL;
    const char* sep = filename + strlen(PROTO_NAME) + 1;
    if (!sep || !*sep)
        return AVERROR_EXTERNAL;
    c->url = av_strdup(sep);

    C_DataSourceOptions ds_opts = ffp->data_source_opt;
    ds_opts.download_options.interrupt_cb.opaque = h->interrupt_callback.opaque;
    ds_opts.download_options.interrupt_cb.callback = h->interrupt_callback.callback;
    ds_opts.download_options.headers = c->headers;
    ds_opts.download_options.user_agent = c->user_agent;
    if (c->enable_cache && c->seg_index >= 0 && ffp->enable_segment_cache) {
        ds_opts.type = kDataSourceTypeAsyncV2;
    }
    ds_opts.download_options.player_statistic = ffp->player_statistic;

    ds_opts.download_options.context_id = ffp->session_id;
    snprintf(ds_opts.download_options.session_uuid, SESSION_UUID_BUFFER_LEN, "%s", ffp->session_uuid);
    snprintf(ds_opts.datasource_extra_msg, DATASOURCE_OPTION_EXTRA_BUFFER_LEN, "hls_segment/%lld-%lld", c->start_offset, c->end_offset);
    c->data_source = ac_data_source_create(ds_opts,
                                           ffp->cache_session_listener, ffp->cache_callback, &ffp->cache_stat.ac_runtime_info);
    char* cache_key = NULL;
    if (c->seg_index >= 0) {
        if (!c->host_cache_key) {
            c->host_cache_key = hodor_generate_cache_key(c->host_url);
        }
        cache_key = create_segment_cache_key(c->host_cache_key, c->seg_index);
    }
    int64_t ret = ac_data_source_open(c->data_source, c->url, cache_key, c->start_offset, c->end_offset ? (c->end_offset - c->start_offset) : kLengthUnset, true);
    av_free(cache_key);
    c->position = c->start_offset;
    if (ret > 0 || ret == kLengthUnset) {
        c->total_size = ret;
        if (ret == kLengthUnset && c->end_offset >= 0 && c->start_offset > 0) {
            c->total_size = c->end_offset - c->start_offset;
        }
        ffp->cache_stat.ffmpeg_adapter_qos.adapter_error = 0;
        return 0;
    } else {
        if (ret == 0) {
            c->adapter_qos->adapter_error = kResultAdapterOpenNoData;
        } else {
            c->adapter_qos->adapter_error = (int)ret;
        }
        ijkdatasource_close(h);
        return AVERROR_EXIT;
    }
}

static int ijkdatasource_read(URLContext* h, unsigned char* buf, int size) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGV("[%s:%d] url:%s size:%d", __func__, __LINE__, c->url, size);
    int64_t  read_len = 0;
    if (size <= 0) {
        ALOGE("[%s], Invalid params. size:%d", __func__, size);
        return size;
    }
    read_len = ac_data_source_read(c->data_source, buf, 0, size);
    if (read_len == 0) {
        ALOGE("[%s], ret_or_len == 0, return kResultAdapterReadNoData", __func__);
        if (c->adapter_qos) {
            c->adapter_qos->adapter_error = kResultAdapterOpenNoData;
        }
        return AVERROR_EXIT;
    } else if (read_len < 0) {
        if (c->adapter_qos) {
            c->adapter_qos->adapter_error = (int)read_len;
        }
        if (read_len == kResultEndOfInput) {
            ALOGE("[%s], ret_or_len = kResultEndOfInput, return %d AVERROR_EOF \n", __func__, AVERROR_EOF);
            return AVERROR_EOF;
        } else {
            ALOGE("[%s], ret_or_len = %d, return AVERROR_EXIT", __func__, read_len);
            return AVERROR_EXIT;
        }
    } else {
        c->position  += read_len;
        c->adapter_qos->total_read_bytes += read_len;
        return (int) read_len;
    }
}

static int64_t ijkdatasource_seek(URLContext* h, int64_t offset, int whence) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGD("[%s:%d] url:%s offset:%lld whence:%d", __func__, __LINE__, c->url, offset, whence);

    int64_t pos = 0;
    int64_t end_pos = c->end_offset > 0 ? c->end_offset : c->total_size;
    if (whence == AVSEEK_SIZE) {
        int64_t ret = c->total_size;
        if (ret < 0) {
            ALOGE("%s, whence:AVSEEK_SIZE,return -1, coz totalsize is:%d\n", __func__, ret);
            return -1;
        } else {
            return ret;
        }
    } else if (whence == SEEK_SET) {
        pos = offset;
    } else if (whence == SEEK_CUR) {
        pos = c->position + offset;
        if (end_pos > 0 && pos > end_pos) {
            pos = end_pos;
        }
    } else if (whence == SEEK_END) {
        pos = end_pos;
    }
    if (c->total_size > 0) {
        if (pos > end_pos) {
            if (c->adapter_qos) {
                c->adapter_qos->adapter_error = kResultAdapterSeekOutOfRange;
            }
            ALOGE("[%s], pos（%lld) > c->total_size(%lld), c->start_offset:%lld, c->end_offset:%lld, return AVERROR_EOF \n",
                  __func__, pos, c->total_size, c->start_offset, c->end_offset);
            return AVERROR_EOF;
        } else if (pos == end_pos) {
            // 有的视频会seek到最后一个字节，后续也不会做什么，这个直接返回当前位置即可兼容
            return c->position;
        } else {
            // continue to do seek
        }
    }

//    int64_t ret = ac_data_source_seek(c->data_source, pos - c->start_offset);
    if (pos < 0 || pos < c->start_offset || (pos > c->end_offset && c->end_offset > 0)) {
        ALOGE("[%s], pos（%lld) > c->total_size(%lld), c->start_offset:%lld, c->end_offset:%lld, seek pos out of bound \n",
              __func__, pos, c->total_size, c->start_offset, c->end_offset);
        return -1;
    }
    if (c->position == pos) {
        return pos;
    }
    int64_t ret = ac_data_source_seek(c->data_source, pos);
    if (kVerbose)
        ALOGD("[%s], position:%lld , offset:%lld, pos:%lld, ret:%lld, total_size:%lld whence:%d, 《《《 \n",
              __func__, c->position, offset, pos, ret, c->total_size, whence);

    if (ret < 0) {
        ALOGE("%s, ac_data_source_seek %lld\n", __func__, pos);
        if (c->adapter_qos) {
            c->adapter_qos->adapter_error = (int)ret;
        }
        return AVERROR_EXIT;
    } else {
        c->position = pos;
    }

    return pos;
}

#define OFFSET(x) offsetof(Context, x)
#define D AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    {
        "ffplayer-opaque",          "a pointer to ffplayer",
        OFFSET(opaque),             IJKAV_OPTION_INT64(0, INT64_MIN, INT64_MAX)
    },
    {
        "user_agent", "override User-Agent header",
        OFFSET(user_agent), IJKAV_OPTION_STR(NULL)
    },
    {
        "user-agent", "override User-Agent header",
        OFFSET(user_agent), IJKAV_OPTION_STR(NULL)
    },
    {
        "http_proxy", "set HTTP proxy to tunnel through",
        OFFSET(http_proxy), IJKAV_OPTION_STR(NULL)
    },
    {
        "headers", "set custom HTTP headers, can override built in default headers",
        OFFSET(headers), IJKAV_OPTION_STR(NULL)
    },
    {
        "cookies", "set cookies to be sent in applicable future requests, use newline delimited Set-Cookie HTTP field value syntax",
        OFFSET(cookies), IJKAV_OPTION_STR(NULL)
    },
    {
        "host_url", "set parent url, such as parent playlist url",
        OFFSET(host_url), IJKAV_OPTION_STR(NULL)
    },
    {
        "offset", "url range start pos",
        OFFSET(start_offset), IJKAV_OPTION_INT64(0, INT64_MIN, INT64_MAX)
    },
    {
        "end_offset", "url range end pos",
        OFFSET(end_offset), IJKAV_OPTION_INT64(0, INT64_MIN, INT64_MAX)
    },
    {
        "enable_cache", "enable cache",
        OFFSET(enable_cache), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, AV_OPT_FLAG_DECODING_PARAM
    },
    {
        "seg_index", "segment index",
        OFFSET(seg_index), IJKAV_OPTION_INT(-1, INT_MIN, INT_MAX)
    },
    {
        "host_cache_key", "cache key for url",
        OFFSET(host_cache_key), IJKAV_OPTION_STR(NULL)
    },
    { NULL }
};

#undef D
#undef OFFSET

static const AVClass ijkdatasource_context_class = {
    .class_name = "IjkHttpDataSource",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

URLProtocol ijkff_ijkdatasource_protocol = {
    .name                = PROTO_NAME,
    .url_open2           = ijkdatasource_open,
    .url_read            = ijkdatasource_read,
    .url_seek            = ijkdatasource_seek,
    .url_close           = ijkdatasource_close,
    .priv_data_size      = sizeof(Context),
    .priv_data_class     = &ijkdatasource_context_class,
};
