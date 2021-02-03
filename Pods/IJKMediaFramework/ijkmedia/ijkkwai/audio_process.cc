#if defined(CONFIG_KS_AUDIOPROCESS) || defined(__APPLE__)

#include <ijkmedia/ijkplayer/ff_ffinc.h>
#include <string>
#include "c_audio_process.h"
#include "audio_engine/interface/audio_dsp_interface.h"

void AudioProcessor_init(void** audio_processor, int sample_rate, int channels, char* audioStr) {
    if (*audio_processor) {
        return;
    }

    std::string audioString(audioStr);
    *audio_processor = kuaishou::audioprocesslib::CreateAudioVideoPlayerProcessor(sample_rate, channels, audioString);
}

void AudioProcessor_process(void** audio_processor, char* data, int channels, int sample_cnt) {
    if (*audio_processor == nullptr) {
        return;
    }

    kuaishou::audioprocesslib::AudioVideoPlayerProcessorInterface* pCAudioVideoPlayerProcessor =
        (kuaishou::audioprocesslib::AudioVideoPlayerProcessorInterface*)(*audio_processor);

    short* pcmData = (short*)data;
    while (sample_cnt >= AUDIODATAFRAME) {
        pCAudioVideoPlayerProcessor->Process((short*) pcmData, AUDIODATAFRAME);
        sample_cnt -= AUDIODATAFRAME;
        pcmData += AUDIODATAFRAME * channels;
    }

    if (sample_cnt) {
        pCAudioVideoPlayerProcessor->Process((short*) pcmData, sample_cnt);
    }
}

void AudioProcessor_releasep(void** audio_processor) {
    if (*audio_processor == nullptr) {
        return;
    }

    kuaishou::audioprocesslib::AudioVideoPlayerProcessorInterface* pCAudioVideoPlayerProcessor =
        (kuaishou::audioprocesslib::AudioVideoPlayerProcessorInterface*)(*audio_processor);

    delete pCAudioVideoPlayerProcessor;

    *audio_processor = nullptr;
}

void AudioCompressProcessor_init(void** audio_compress_processor, int sample_rate, int channels, float make_up_gain, float normalnize_gain) {
    if (*audio_compress_processor) {
        return;
    }

    *audio_compress_processor = kuaishou::audioprocesslib::CreateAudioVideoCompressProcessor(sample_rate, channels, make_up_gain, normalnize_gain);
}

void AudioCompressProcessor_process(void** audio_compress_processor, char* data, int channels, int sample_cnt) {
    if (*audio_compress_processor == nullptr) {
        return;
    }

    kuaishou::audioprocesslib::AudioVideoCompressProcessorInterface* pCAudioCompressProcessor =
        (kuaishou::audioprocesslib::AudioVideoCompressProcessorInterface*)(*audio_compress_processor);

    short* pcmData = (short*)data;
    while (sample_cnt >= AUDIODATAFRAME) {
        pCAudioCompressProcessor->Process((short*) pcmData, AUDIODATAFRAME);
        sample_cnt -= AUDIODATAFRAME;
        pcmData += AUDIODATAFRAME * channels;
    }

    if (sample_cnt) {
        pCAudioCompressProcessor->Process((short*) pcmData, sample_cnt);
    }
}

void AudioCompressProcessor_releasep(void** audio_compress_processor) {
    if (*audio_compress_processor == nullptr) {
        return;
    }

    kuaishou::audioprocesslib::AudioVideoCompressProcessorInterface* pCAudioCompressProcessor =
        (kuaishou::audioprocesslib::AudioVideoCompressProcessorInterface*)(*audio_compress_processor);
    delete pCAudioCompressProcessor;

    *audio_compress_processor = nullptr;
}

void AudioSpectrumProcessor_init(void** audio_processor, int sample_rate, int channels) {
    if (*audio_processor) {
        return;
    }

    *audio_processor = kuaishou::audioprocesslib::CreateAudioPlayerSpectrumProcessor(sample_rate, channels);
}

void AudioSpectrumProcessor_process(void** audio_processor, char* data, int channels, int sample_cnt, float* spectrum_volume) {
    if (*audio_processor == nullptr) {
        return;
    }

    kuaishou::audioprocesslib::AudioPlayerSpectrumInterface* pCAudioPlayerSpectrumProcessor =
        (kuaishou::audioprocesslib::AudioPlayerSpectrumInterface*)(*audio_processor);

    short* pcmData = (short*)data;
    if (sample_cnt >= AUDIODATAFRAME * 2) {
        sample_cnt = AUDIODATAFRAME * 2;
    }

    pCAudioPlayerSpectrumProcessor->Process(spectrum_volume, AUDIOFFTSIZE, pcmData, sample_cnt);
}

void AudioSpectrumProcessor_releasep(void** audio_processor) {
    if (*audio_processor == nullptr) {
        return;
    }

    kuaishou::audioprocesslib::AudioPlayerSpectrumInterface* pCAudioPlayerSpectrumProcessor =
        (kuaishou::audioprocesslib::AudioPlayerSpectrumInterface*)(*audio_processor);

    delete pCAudioPlayerSpectrumProcessor;

    *audio_processor = nullptr;
}

#endif