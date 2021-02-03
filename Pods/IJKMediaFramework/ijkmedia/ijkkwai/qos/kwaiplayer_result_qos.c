#include <stdio.h>
#include <memory.h>
#include "kwaiplayer_result_qos.h"
#include "ff_ffplay.h"

typedef struct KwaiPlayerResultQos {
    float videoAvgFps;
    char* videoStatJson;
    char* briefVideoStatJson;
} KwaiPlayerResultQos;

KwaiPlayerResultQos* KwaiPlayerResultQos_create() {
    KwaiPlayerResultQos* qos = mallocz(sizeof(KwaiPlayerResultQos));
    return qos;
}

void KwaiPlayerResultQos_releasep(KwaiPlayerResultQos** pp) {
    if (!pp || !*pp) {
        return;
    }
    KwaiPlayerResultQos* qos = *pp;
    if (qos->videoStatJson) {
        freep((void**)&qos->videoStatJson);
    }

    if (qos->briefVideoStatJson) {
        freep((void**)&qos->briefVideoStatJson);
    }

    free(qos);
    *pp = NULL;
}

void KwaiPlayerResultQos_collect_result_qos(KwaiPlayerResultQos* qos, struct FFPlayer* ffp) {
    if (ffp->islive) {
        // fixme implement me
    } else {
        if (qos->videoStatJson) {
            freep((void**)&qos->videoStatJson);
        }
        qos->videoStatJson = ffp_get_video_stat_json_str(ffp);

        if (qos->briefVideoStatJson) {
            freep((void**)&qos->briefVideoStatJson);
        }
        qos->briefVideoStatJson = ffp_get_brief_video_stat_json_str(ffp);
    }

    qos->videoAvgFps = ffp_get_property_float(ffp, FFP_PROP_FLOAT_VIDEO_AVG_FPS, 0.f);
}

const char* KwaiPlayerResultQos_get_videoStatJson(KwaiPlayerResultQos* qos) {
    return qos->videoStatJson ? qos->videoStatJson : "N/A";
}

const char* KwaiPlayerResultQos_get_briefVideoStatJson(KwaiPlayerResultQos* qos) {
    return qos->briefVideoStatJson ? qos->briefVideoStatJson : "N/A";
}

float KwaiPlayerResultQos_get_videoAvgFps(KwaiPlayerResultQos* qos) {
    return qos->videoAvgFps;
}
