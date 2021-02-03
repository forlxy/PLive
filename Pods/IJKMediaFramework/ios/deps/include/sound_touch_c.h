#ifndef __SOUND_TOUCH_C__
#define __SOUND_TOUCH_C__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SoundTouchC_t {
    void *context_;
} SoundTouchC;

// return 0: fail; non-zero: ok
int SoundTouchC_init(SoundTouchC **st);
void SoundTouchC_free(SoundTouchC *st);

void SoundTouchC_setSampleRate(SoundTouchC *st, int rate);
void SoundTouchC_setChannels(SoundTouchC *st, int nums);
void SoundTouchC_setRate(SoundTouchC *st, float rate);
void SoundTouchC_setRateChange(SoundTouchC *st, float rate);
void SoundTouchC_setTempo(SoundTouchC *st, float tempo);
void SoundTouchC_setTempoChange(SoundTouchC *st, int tempo);
void SoundTouchC_setPitch(SoundTouchC *st, float pitch);
void SoundTouchC_setPitchOctaves(SoundTouchC *st, float pitch);
void SoundTouchC_setPitchSemiTones(SoundTouchC *st, float pitch);
// return <=0: fail; >0: output sample count
int SoundTouchC_processData(SoundTouchC *st, short *buf_in, int sample_cnt_in,
                            short *buf_out, int sample_cnt_out);

#ifdef __cplusplus
}
#endif

#endif /* __SOUND_TOUCH_C__ */

