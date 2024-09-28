#ifndef _AS_AUD_SPEAK_H_
#define _AS_AUD_SPEAK_H_

#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUTH_VOICE_FILE     "../resource/auth.wav"
#define WAKEUP_VOICE_FILE   "../resource/wakeup.wav"

void *as_audio_SpeakThr(void *arg);
int as_mixer_init();
int as_mixer_SetSpeakerVolume(long volume);
int as_mixer_GetSpeakerVolume(long *lval, long *rval);
int as_mixer_DecreaseSpeakerVolume();
int as_mixer_IncreaseSpeakerVolume();

void *as_audio_PvSpeakThr(void *arg);
#ifdef __cplusplus
}
#endif

#endif