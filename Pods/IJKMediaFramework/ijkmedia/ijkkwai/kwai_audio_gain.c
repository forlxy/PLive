#include <stdlib.h>
#include <string.h>
#include "kwai_audio_gain.h"

/* [ags=0.464610][agm=0.744617][agsi=2000] */
int audio_gain_parse_comment(char* config, const char* key, float* value) {
    char* ptr = NULL, *pre = NULL;

    if (!config || !key) {
        return -1;
    }

    int len = strlen(key);
    ptr = strstr(config, key);

    //move ptr to skip key.
    while (ptr && len) {
        ptr++;
        len--;
    }

    pre = ptr;
    while (ptr && *ptr != ']') {
        ptr++;
    }
    if (ptr && *ptr == ']') {
        *ptr = 0;
        *value = (float)strtod(pre, NULL);
        *ptr = ']';
        return 0;
    }

    return -1;
}

void AudioGain_reset(AudioGain* as) {
    as->make_up_gain = 0;
    as->normalnize_gain = 0;
    as->audio_processor = NULL;
    as->audio_compress_processor = NULL;
    as->enabled = false;
    as->enable_audio_compress = false;
}

//get audioCmpGain and eqMode from metadata comment
void AudioGain_parse_config(AudioGain* as, char* config) {
    if (audio_gain_parse_comment(config, "makeupGain=", &as->make_up_gain) < 0) {
        return;
    }
    if (audio_gain_parse_comment(config, "normalnizeGain=", &as->normalnize_gain) < 0) {
        return;
    }
    as->enable_audio_compress = true;
    return;
}

