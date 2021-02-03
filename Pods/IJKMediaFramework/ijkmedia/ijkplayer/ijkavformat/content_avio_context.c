//
//  ContentAvioContext.c
//  IJKMediaFramework
//
//  Created by 李金海 on 2019/7/5.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#include "content_avio_context.h"
#include "ijksdl/ijksdl.h"

struct ContentAvioContext {
    char* content;
    int64_t total_size;
    int64_t pos;
};

static int ContentAVIOContext_read(void* context, uint8_t* buf, int size) {
    struct ContentAvioContext* c = (struct ContentAvioContext*)context;
    if (size <= 0) {
        ALOGE("[%s], Invalid params. size:%d", __func__, size);
        return size;
    }

    if (c->total_size <= 0) {
        ALOGE("[%s], Invalid total_size :%d", __func__, c->total_size);
        return 0;
    }

    char* content = c->content;

    if (c->pos < 0)
        c->pos = 0;

    if (c->pos >= c->total_size) {
        ALOGE("[%s], return kResultAdapterReadNoData", __func__);
        return AVERROR_EOF;
    }

    int64_t remaining_len = c->total_size - c->pos;
    int read_len = size <= remaining_len ? size : (int)remaining_len;
    strncpy((char*)buf, content + c->pos, read_len);
    c->pos += read_len;

    return read_len;
}

#define FILE_READ_BUFFER_MAX_LEN (4*1024)
AVIOContext* ContentAVIOContext_create(char* content) {
    struct ContentAvioContext* context = av_mallocz(sizeof(struct ContentAvioContext));
    context->content = content;
    context->total_size = strlen(content);
    unsigned char* aviobuffer = (unsigned char*) av_malloc(FILE_READ_BUFFER_MAX_LEN);
    AVIOContext* avio = avio_alloc_context(aviobuffer, FILE_READ_BUFFER_MAX_LEN, 0,
                                           context,
                                           ContentAVIOContext_read,
                                           NULL,
                                           NULL);
    return avio;
}

void ContentAVIOContext_releasep(AVIOContext** pp_avio) {
    if (!pp_avio || !*pp_avio) {
        return;
    }
    AVIOContext* avio = *pp_avio;
    av_freep(&avio->opaque);
    avio_closep(pp_avio);
}
