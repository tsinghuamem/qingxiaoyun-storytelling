#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <assert.h>

#include "MultiTimer.h"
#include "as_config.h"
#include "as_mgr.h"
#include "ringbuf.h"
#include "as_msg_def.h"
#include "as_aud_speak.h"

static long minVolume = 0;
static long maxVolume = 100;
static snd_mixer_t *mixerHandle = NULL;
static snd_mixer_elem_t *elemHandle = NULL;

const float VOLUME_STEP = 10;

int as_mixer_init()
{
    if (mixerHandle != NULL) {
        as_log_Debug("Mixer has already init.\n");
        return 0;
    }
    
    // 1. Open mixer
    snd_mixer_open(&mixerHandle, 0);
    if (mixerHandle == NULL) {
        as_log_Err("Failed to open mixer.\n");
        return -1;
    }
    
    // 2. Attach HCTL to opened mixer
    snd_mixer_attach(mixerHandle, AUDIO_MIXER_DEV);
    
    // 3. Register mixer simple element class.
    snd_mixer_selem_register(mixerHandle, NULL, NULL);
    
    // 4. Load mixer.
    snd_mixer_load(mixerHandle);
    
    // 5. Get speaker element.
    elemHandle = snd_mixer_first_elem(mixerHandle);
    while (elemHandle != NULL) {
        if (strcmp("Speaker", snd_mixer_selem_get_name(elemHandle)) == 0) {
            as_log_Debug("Get elem name:%s\n", snd_mixer_selem_get_name(elemHandle));
            break;
        }
        elemHandle = snd_mixer_elem_next(elemHandle);
    }
    
    if (elemHandle == NULL) {
        as_log_Err("Failed to get selem.\n");
        snd_mixer_close(mixerHandle);
        mixerHandle = NULL;
        return -1;        
    }
    
    snd_mixer_selem_get_playback_volume_range(elemHandle, &minVolume, &maxVolume);
    as_log_Debug("volume range: %ld -- %ld\n", minVolume, maxVolume);
    
    return 0;
}

int as_mixer_SetSpeakerVolume(long volume)
{
    long alsaVolume = volume * (maxVolume - minVolume) / 100 ;
    
    if (elemHandle == NULL) {
        as_log_Err("elem handle is NULL.\n");
        return -1;
    }
    
    if (snd_mixer_selem_set_playback_volume_all(elemHandle, alsaVolume)) {
        if (mixerHandle != NULL) {
            snd_mixer_close(mixerHandle);
            mixerHandle = NULL;
        }
        return -1;
    }
    
    return 0;
}

int as_mixer_GetSpeakerVolume(long *lval, long *rval)
{    
    snd_mixer_handle_events(mixerHandle);
    
    if (lval != NULL) {
        snd_mixer_selem_get_playback_volume(elemHandle, SND_MIXER_SCHN_FRONT_LEFT, lval);
        as_log_Debug("currnet volume: leftVal = %ld\n", lval);
    }
    
    if (rval != NULL) {
        snd_mixer_selem_get_playback_volume(elemHandle, SND_MIXER_SCHN_FRONT_RIGHT, rval);
        as_log_Debug("currnet volume: rightVal = %ld\n", rval);        
    }

    return 0;
}

int as_mixer_DecreaseSpeakerVolume()
{
    long newVolume = 0;
    long lval, rval;
    
    as_mixer_GetSpeakerVolume(&lval, &rval);
    
    if (lval >= 0 + VOLUME_STEP) {
        lval -= VOLUME_STEP;
    } else {
        lval = 0;
    }

    if (rval >= 0 + VOLUME_STEP) {
        rval -= VOLUME_STEP;
    } else {
        rval = 0;
    }

    newVolume = lval > rval ? lval : rval;
    
    as_mixer_SetSpeakerVolume(newVolume);
    
    return 0;
}

int as_mixer_IncreaseSpeakerVolume()
{
    long newVolume = 0;
    long lval, rval;
    
    as_mixer_GetSpeakerVolume(&lval, &rval);
    
    if (lval <= 100 - VOLUME_STEP) {
        lval += VOLUME_STEP;
    } else {
        lval = 100;
    }

    if (rval <= 100 - VOLUME_STEP) {
        rval += VOLUME_STEP;
    } else {
        rval = 100;
    }

    newVolume = lval < rval ? lval : rval;
    
    as_mixer_SetSpeakerVolume(newVolume);
    
    return 0;
}

void *as_audio_SpeakThr(void *arg)
{
    int ret = 0;
    size_t len, off;
    int size;    
    snd_pcm_t *handle;    
    snd_pcm_hw_params_t *params;    
    unsigned int val;    
    int dir;    
    snd_pcm_uframes_t frames;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;
    
    as_mixer_init();

    /* Open PCM device for playback */
    ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        as_log_Err("Failed to open pcm device:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }

    snd_pcm_hw_params_alloca(&params);
    
    ret = snd_pcm_hw_params_any(handle, params);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_any:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }
    
    ret = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_access:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }
    
    ret = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_format:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }    
    
    ret = snd_pcm_hw_params_set_channels(handle, params, 2);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_channels:%s.\n", snd_strerror(ret));
        return (void*)-1;
    } 
    
    val = 44100;
    ret = snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_rate_near:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }

    frames = 32;
    ret = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_period_size_near:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }

    ret = snd_pcm_hw_params(handle, params);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params:%s.\n", snd_strerror(ret));
        return (void*)-1;
    }    
    
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    as_log_Debug("Get period size is %d", frames);
    
    size = frames * 4; /* 2 bytes/sample, 2 channels */
    //snd_pcm_hw_params_get_period_time(params, &val, &dir);
        
    while (!mgrPtr->quit) {
        len = ringbuf_consume(mgrPtr->receiveRingBuf, &off);
        if (len > 0) {
            size_t vlen = 0;
            while (vlen < len) {
                as_msg_hdr_t *msgHdrPtr = (as_msg_hdr_t*)(&(mgrPtr->receiveBuf[off]));

                off += sizeof(as_msg_hdr_t) + msgHdrPtr->len;
                vlen += sizeof(as_msg_hdr_t) + msgHdrPtr->len;
                
                // Decode...
                
                // Playback.
                ret = snd_pcm_writei(handle, msgHdrPtr->payload, frames);
                if (ret == -EPIPE) {
                    as_log_Err("underrun occured\n");
                } else if (ret < 0) {
                    as_log_Err("error from writei: %s\n", snd_strerror(ret));
                }
            }
        }
        else {
            usleep(1000);
            continue;
        }        
    }
    
    snd_pcm_drain(handle);    
    snd_pcm_close(handle);
    
    return (void*)0;
}

// Using pvspeaker to implement
#include "pv_speaker.h"

static pv_speaker_t *speaker = NULL;
static int8_t *auth_pcm_data = NULL;
static uint32_t auth_pcm_num_samples = 0;

static int8_t *waken_pcm_data = NULL;
static uint32_t waken_pcm_num_samples = 0;

static int as_audio_VoiceDataInit(uint32_t bytes_per_sample)
{
    FILE *authfile = fopen(AUTH_VOICE_FILE, "rb");
    if (!authfile) {
        perror("Unable to open file");
        return -1;
    }

    fseek(authfile, 0, SEEK_END);
    long size = ftell(authfile);
    fseek(authfile, 0, SEEK_SET);

    auth_pcm_data = malloc(size);
    if (auth_pcm_data == NULL) {
        perror("Unable to open file");
        return -1;
    }

    fread(auth_pcm_data, size, 1, authfile);
    auth_pcm_num_samples = size / bytes_per_sample;

    FILE *wakenfile = fopen(WAKEUP_VOICE_FILE, "rb");
    if (!wakenfile) {
        perror("Unable to open file");
        return -1;
    }

    fseek(wakenfile, 0, SEEK_END);
    size = ftell(wakenfile);
    fseek(wakenfile, 0, SEEK_SET);

    waken_pcm_data = malloc(size);
    if (waken_pcm_data == NULL) {
        perror("Unable to open file");
        return -1;
    }

    fread(waken_pcm_data, size, 1, wakenfile);
    waken_pcm_num_samples = size / bytes_per_sample;

    return 0;
}

static void as_speak_welcome(uint16_t bits_per_sample)
{
    pv_speaker_status_t status;
    int32_t total_written_length = 0;

    as_log_Debug("speak welcome, num sample is %lu...\n", auth_pcm_num_samples);
    while (total_written_length < auth_pcm_num_samples) {
        int32_t written_length = 0;
        pv_speaker_status_t status = pv_speaker_write(
                speaker,
                &auth_pcm_data[total_written_length * bits_per_sample / 8],
                auth_pcm_num_samples - total_written_length,
                &written_length);
        if (status != PV_SPEAKER_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to write with %s.\n", pv_speaker_status_to_string(status));
            return;
        }
        total_written_length += written_length;
    }

    int32_t written_length = 0;
    status = pv_speaker_flush(speaker, auth_pcm_data, 0, &written_length);
    if (status != PV_SPEAKER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to flush pcm with %s.\n", pv_speaker_status_to_string(status));
        return;
    }
}

static void as_speak_honey(uint16_t bits_per_sample)
{
    pv_speaker_status_t status;
    int32_t total_written_length = 0;

    as_log_Debug("speak honey, waken_pcm_num_samples is %lu...\n", waken_pcm_num_samples);
    while (total_written_length < waken_pcm_num_samples) {
        int32_t written_length = 0;
        pv_speaker_status_t status = pv_speaker_write(
                speaker,
                &waken_pcm_data[total_written_length * bits_per_sample / 8],
                waken_pcm_num_samples - total_written_length,
                &written_length);
        if (status != PV_SPEAKER_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to write with %s.\n", pv_speaker_status_to_string(status));
            return;
        }
        total_written_length += written_length;
    }

    status = pv_speaker_flush(speaker, waken_pcm_data, 0, &total_written_length);
    if (status != PV_SPEAKER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to flush pcm with %s.\n", pv_speaker_status_to_string(status));
        return;
    }
}

static void as_speak_write_s16(int8_t *pcm, int32_t pcm_length)
{
    pv_speaker_status_t status;
    int32_t total_written_length = 0;
    int32_t num_samples = pcm_length / 2;
    while (total_written_length < num_samples) {
        int32_t written_length = 0;
        pv_speaker_status_t status = pv_speaker_write(
                speaker,
                &pcm[total_written_length * 2], // S16
                pcm_length - total_written_length,
                &written_length);
        if (status != PV_SPEAKER_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to write with %s.\n", pv_speaker_status_to_string(status));
            return;
        }
        total_written_length += written_length;
    }
#if 0
    int32_t written_length = 0;
    status = pv_speaker_flush(speaker, pcm, 0, &written_length);
    if (status != PV_SPEAKER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to flush pcm with %s.\n", pv_speaker_status_to_string(status));
        return;
    }
#endif
}

void as_wakeup_TimerHandler(MultiTimer *timer, void *userData)
{
    as_mgr_t *mgrPtr = (as_mgr_t*)userData;
    as_log_Debug("wakeup timer fired at %lu ms\n", as_platform_GetTicks());

    mgrPtr->state = AS_SM_TRANSLATE;
}

void *as_audio_PvSpeakThr(void *arg)
{
    int ret = 0;
    uint32_t sample_rate = 22050;
    uint16_t bits_per_sample = 16;
    int32_t buffer_size_secs = 20;
    int32_t device_index = SPEAKER_DEVICE_ID;
    size_t len, off;
    MultiTimer recordTimer;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;

    as_mixer_init();

    ret = as_audio_VoiceDataInit(bits_per_sample/8);
    if (ret != 0) {
        as_log_Err("Failed to init promt voice data\n");
        return (void*)-1;
    }

    pv_speaker_status_t status = pv_speaker_init(
            sample_rate,
            bits_per_sample,
            buffer_size_secs,
            device_index,
            &speaker);
	if (status != PV_SPEAKER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize device with %s.\n", pv_speaker_status_to_string(status));
        return (void*)-1;
	}

    const char *selected_device = pv_speaker_get_selected_device(speaker);
    fprintf(stdout, "Selected device: %s.\n", selected_device);

    status = pv_speaker_start(speaker);
    if (status != PV_SPEAKER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to start device with %s.\n", pv_speaker_status_to_string(status));
        return (void*)-1;
    }

    while (!mgrPtr->quit) {
        if (mgrPtr->state == AS_SM_AUTH) {
            as_speak_welcome(bits_per_sample);
            mgrPtr->state = AS_SM_IDLE;
        }

        if (mgrPtr->state == AS_SM_WAKEUP) {
            as_speak_honey(bits_per_sample);
            mgrPtr->state = AS_SM_RECORDING;
            
            // Start the timer for recording.
            multiTimerStart(&recordTimer, AS_RECORD_TIMER_INTERVAL,
                as_wakeup_TimerHandler, (void*)mgrPtr);
        }

        if (mgrPtr->state == AS_SM_SPEAKING) {
            // Read the voice data from AI agent and playback.
            len = ringbuf_consume(mgrPtr->receiveRingBuf, &off);
            assert(off < AUDIO_BUFFER_SIZE);
            
            //as_log_Debug("Read receive ring buffer: get %llu bytes at %llu\n", len, off);
            if (len > 0) {
                size_t vlen = 0;
                while (vlen < len) {
                    as_msg_hdr_t *msgHdrPtr = (as_msg_hdr_t*)(&(mgrPtr->receiveBuf[off]));

                    // Decode if the data is not raw pcm.
                    // TODO...
                    
                    // Playback.
                    as_speak_write_s16(msgHdrPtr->payload, msgHdrPtr->len);

                    off += sizeof(as_msg_hdr_t) + msgHdrPtr->len;
                    vlen += sizeof(as_msg_hdr_t) + msgHdrPtr->len;
                }
                
            }
            else {
                usleep(1000);
                continue;
            }
            ringbuf_release(mgrPtr->receiveRingBuf, len);
        }
        else {
            usleep(1000);
        }        
    }

    pv_speaker_stop(speaker);
    pv_speaker_delete(speaker);
    multiTimerStop(&recordTimer);
    
    return (void*)0;
}
