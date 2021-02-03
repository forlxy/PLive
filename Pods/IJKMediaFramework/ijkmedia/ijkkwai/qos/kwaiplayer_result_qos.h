
#pragma once

/**
 * 这个类主要是在player完全stop停止下来后，承载一些结果qos的数据
 */
typedef struct KwaiPlayerResultQos KwaiPlayerResultQos;

KwaiPlayerResultQos* KwaiPlayerResultQos_create();
void KwaiPlayerResultQos_releasep(KwaiPlayerResultQos** pp);

struct FFPlayer;
/**
 * 如果是live则会收集live的Qos数据，如果是vod则会收集vod的，目前只支持vod
 */
void KwaiPlayerResultQos_collect_result_qos(KwaiPlayerResultQos* qos, struct FFPlayer* ffp);
const char* KwaiPlayerResultQos_get_videoStatJson(KwaiPlayerResultQos* qos);
const char* KwaiPlayerResultQos_get_briefVideoStatJson(KwaiPlayerResultQos* qos);
float KwaiPlayerResultQos_get_videoAvgFps(KwaiPlayerResultQos* qos);