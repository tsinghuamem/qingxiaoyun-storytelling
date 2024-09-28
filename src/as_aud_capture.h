#ifndef _AS_AUD_CAPTURE_H_
#define _AS_AUD_CAPTURE_H_

#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern pthread_mutex_t recordMutex;
extern int16_t *recordAudDataPtr;
extern size_t recordAudDataLen;

#define MAX_RECORD_AUDIO_DATA       (1 * 1024 * 1024)

void *as_audio_CaptureThr(void *arg);
void *as_audio_PvRecordThr(void *arg);

#ifdef __cplusplus
}
#endif

#endif