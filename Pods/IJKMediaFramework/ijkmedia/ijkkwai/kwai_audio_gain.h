#ifndef KWAI_AUDIO_GAIN_H
#define KWAI_AUDIO_GAIN_H

#include <stdbool.h>

enum DEVIDE_OUT_TYPE {
    DEVICE_OUT_UNKNOWN,
    DEVICE_OUT_SPEAKER,
};

typedef struct AudioGain {
    float make_up_gain;//make_up_gain/normalnize_gain表示要调整的音量目标增益值
    float normalnize_gain;
    int enabled;
    bool enable_audio_compress;
    char* audio_str;
    void* audio_processor;
    void* audio_compress_processor;
} AudioGain;

int audio_gain_parse_comment(char* config, const char* key, float* value);
void AudioGain_reset(AudioGain* as);
void AudioGain_parse_config(AudioGain* as, char* config);

#endif /* KWAI_AUDIO_GAIN_H */
