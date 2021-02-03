/**
 * 音频频谱，输入音频的sample数据，输出频谱结果，应用场景里会把频谱数据返给app层，渲染出来
 */

#pragma once

#define AUDIOFFTSIZE    256
#define AUDIODATAFRAME 4096

#ifdef __cplusplus
extern "C" {
#endif

//音量调整,目前用于外放场景中
void AudioProcessor_init(void** audio_processor, int sample_rate, int channels, char* audioStr);
void AudioProcessor_process(void** audio_processor, char* data, int channels, int sample_cnt);
void AudioProcessor_releasep(void** audio_processor);

//音量目标值调整，把音量调整到给定的增益
void AudioCompressProcessor_init(void** audio_compress_processor, int sample_rate, int channels, float make_up_gain, float normalnize_gain);
void AudioCompressProcessor_process(void** audio_compress_processor, char* data, int channels, int sample_cnt);
void AudioCompressProcessor_releasep(void** audio_compress_processor);

//用于生成频谱数据
void AudioSpectrumProcessor_init(void** audio_processor, int sample_rate, int channels);
void AudioSpectrumProcessor_process(void** audio_processor, char* data, int channels, int sample_cnt, float* spectrum_volume);
void AudioSpectrumProcessor_releasep(void** audio_processor);
#ifdef __cplusplus
}
#endif
