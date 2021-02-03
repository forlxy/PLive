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

typedef struct ContentAvioContext {
    AVClass*        class;
    int64_t         opaque;
    int64_t         total_size;
    int64_t         pos;
    char*           url;
    char*           user_agent;
    char*           http_proxy;
    char*           headers;
    char*           cookies;
    FfmpegAdapterQos*   adapter_qos;
} Context;

#define CONTENT_PROTO_NAME "content"
static bool kVerbose = true;

char* ijk_index_content_get_hook_url_proto_name() {
    return CONTENT_PROTO_NAME;
}

static int ijk_index_content_close(URLContext* h) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGD("[%s:%d] url:%s", __func__, __LINE__, c->url);
    av_freep(&c->url);
    return 0;
}

static int ijk_index_content_open(URLContext* h, const char* filename, int flags, AVDictionary** options) {
    if (kVerbose)
        ALOGD("[%s:%d] filename:%s, flags:%d \n", __func__, __LINE__, filename, flags);

    if (!filename)
        return -1;

    Context* c = h->priv_data;

    AVIOInterruptCB callback = h->interrupt_callback;
    FFPlayer* ffp = (FFPlayer*)callback.opaque;
    char* content = ffp->index_content.content;

    if (callback.callback(ffp)) {
        return -1;
    }

    assert(ffp);
    c->adapter_qos = &ffp->cache_stat.ffmpeg_adapter_qos;

    c->url = av_strdup(filename);
    c->pos = 0;

    c->total_size = 0;
    if (ffp->index_content.content)
        c->total_size = strlen(content);

    if (!c->total_size) {
        c->adapter_qos->adapter_error = kResultAdapterOpenNoData;
        ALOGE("[%s], return kResultAdapterReadNoData", __func__);
        return AVERROR_EXIT;
    }

    return 0;
}

static int ijk_index_content_read(URLContext* h, unsigned char* buf, int size) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGD("[%s:%d] url:%s size:%d", __func__, __LINE__, c->url, size);

    if (size <= 0) {
        ALOGE("[%s], Invalid params. size:%d", __func__, size);
        return size;
    }

    if (c->total_size <= 0) {
        ALOGE("[%s], Invalid total_size :%d", __func__, c->total_size);
        return 0;
    }

    AVIOInterruptCB callback = h->interrupt_callback;
    FFPlayer* ffp = (FFPlayer*)callback.opaque;
    char* content = ffp->index_content.content;

    if (c->pos < 0)
        c->pos = 0;

    if (c->pos >= c->total_size) {
        ALOGE("[%s], return kResultAdapterReadNoData", __func__);
        if (c->adapter_qos) {
            c->adapter_qos->adapter_error = kResultAdapterOpenNoData;
        }
        return AVERROR_EOF;
    }

    int64_t remaining_len = c->total_size - c->pos;
    int read_len = size <= remaining_len ? size : (int)remaining_len;
    strncpy((char*)buf, content + c->pos, read_len);
    c->pos += read_len;

    return read_len;
}

static int64_t ijk_index_content_seek(URLContext* h, int64_t offset, int whence) {
    Context* c = h->priv_data;
    if (kVerbose)
        ALOGD("[%s:%d] url:%s offset:%lld whence:%d", __func__, __LINE__, c->url, offset, whence);

    if (whence == AVSEEK_SIZE) {
        int64_t ret = c->total_size;
        if (ret < 0) {
            ALOGE("%s, whence:AVSEEK_SIZE,return -1, coz totalsize is:%d\n", __func__, ret);
            return -1;
        } else {
            return ret;
        }
    } else if (whence == SEEK_SET) {
        c->pos = offset;
    } else if (whence == SEEK_CUR) {
        c->pos +=  offset;
    } else if (whence == SEEK_END) {
        c->pos = c->total_size + offset;
    }

    if (c->pos > c->total_size) {
        ALOGE("[%s], pos（%lld) > c->total_size(%lld), return AVERROR_EOF \n", __func__, c->pos, c->total_size);
        if (c->adapter_qos) {
            c->adapter_qos->adapter_error = kResultAdapterSeekOutOfRange;
        }
    }
    return c->pos;
}

#define OFFSET(x) offsetof(Context, x)

static const AVOption options[] = {
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
    { NULL }
};

#undef D
#undef OFFSET

static const AVClass ijk_index_content_context_class = {
    .class_name = "IjkIndexContent",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

URLProtocol ijkff_ijkindexcontent_protocol = {
    .name                = CONTENT_PROTO_NAME,
    .url_open2           = ijk_index_content_open,
    .url_read            = ijk_index_content_read,
    .url_seek            = ijk_index_content_seek,
    .url_close           = ijk_index_content_close,
    .priv_data_size      = sizeof(Context),
    .priv_data_class     = &ijk_index_content_context_class,
};
