#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <alsa/asoundlib.h> 
#include "MultiTimer.h"
#include "pv_recorder.h"
#include "as_mgr.h"
#include "ringbuf.h"
#include "as_msg_def.h"
#include "as_aud_capture.h"
#include "as_config.h"

pthread_mutex_t recordMutex = PTHREAD_MUTEX_INITIALIZER;
int16_t *recordAudDataPtr;
size_t recordAudDataLen;

void *as_audio_CaptureThr(void *arg)
{
    ringbuf_worker_t *worker;
    ssize_t off = -1;
    size_t len;
    int ret;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_access_t accessType = SND_PCM_ACCESS_RW_INTERLEAVED;
    int channels = 2;
    unsigned int val = 44100;    
    int dir;
    int size;
    snd_pcm_uframes_t frames;
    as_msg_hdr_t *msgHdrPtr;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;
    
    worker = ringbuf_register(mgrPtr->captureRingBuf, AUDIO_CAPTURE_WORKER_ID);
    if (worker == NULL) {
        as_log_Err("Failed to create audio capture thread.\n");
        return (void*)-1; 
    }

    ret = snd_pcm_open(&handle, AUDIO_CAPTURE_DEV, SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        as_log_Err("Failed to open pcm device:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }

    // Set pcm params.
    
    // 1. Allocate a hardware parameters object.
    snd_pcm_hw_params_alloca(&params);
    
    // 2. Fill it in with default values.
    ret = snd_pcm_hw_params_any(handle, params);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_any:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }
    
    // 3. Set the desired hardware parameters.
    ret = snd_pcm_hw_params_set_rate_resample(handle, params, 0);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_rate_resample:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }
    
    // 4.Interleaved mode
    ret = snd_pcm_hw_params_set_access(handle, params, accessType);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_access:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }
    
    // 5. Signed 16-bit little-endian format
    ret =  snd_pcm_hw_params_set_format(handle, params, format);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_format:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }

    // 6. Two channels (stereo)
    ret = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_channels:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }
    
    // 7. 44100 bits/second sampling rate (CD quality)
    ret = snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_rate_near:%s.\n", snd_strerror(ret));
        return (void*)-1;         
    }

    // 8. Set period size to 32 frames.
    frames = 32;
    ret = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params_set_period_size_near:%s.\n",
            snd_strerror(ret));
        return (void*)-1;         
    }
    
    // 9. Write the parameters to the driver
    ret = snd_pcm_hw_params(handle, params);
    if (ret < 0) {
        as_log_Err("Failed to call snd_pcm_hw_params:%s.\n",
            snd_strerror(ret));
        return (void*)-1;         
    }

    // Use a buffer large enough to hold one period.
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    
    as_log_Debug("Get period size is %d", frames);
    
    size = frames * 4; /* 2 bytes/sample, 2 channels */
    
    while (!mgrPtr->quit) {
        if (mgrPtr->state != AS_SM_RECORDING) {
            usleep(10000);
            continue;
        }
        
        // Audio recording
        off = ringbuf_acquire(mgrPtr->captureRingBuf, worker, size + sizeof(as_msg_hdr_t));
        if (off < 0) {
            as_log_Err("Ring buffer is full. Failed to get offset.\n");
            usleep(10000);
            continue;
        }
        
        msgHdrPtr = (as_msg_hdr_t*)(&(mgrPtr->captureBuf[off]));
        msgHdrPtr->type = AS_MSG_TYPE_AUDIO;
        msgHdrPtr->len = size;
        
        ret = snd_pcm_readi(handle, msgHdrPtr->payload, frames);
        if (ret == -EPIPE) {
            /* EPIPE means overrun */
            as_log_Err("overrun occurred\n");
            snd_pcm_prepare(handle);
            continue;
        } else if (ret < 0) {
            as_log_Err("error from read: %s\n", snd_strerror(ret));
            continue;
        } else if (ret != (int)frames) {
            as_log_Err("short read, read %d frames\n", ret);
            // Ignore. still write to ring buffer.
        }
        
        ringbuf_produce(mgrPtr->captureRingBuf, worker);
        
        usleep(20000);
    }
    
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
  
    ringbuf_unregister(mgrPtr->captureRingBuf, worker);
    
    return (void*)0;
}

void *as_audio_PvRecordThr(void *arg)
{
    int ret = 0;
    int32_t device_index = MIC4RECORD_DEVICE_ID;
    const int32_t frame_length = 512;
    const int32_t buffer_size_msec = 100;
    ringbuf_worker_t *worker;
    ssize_t off = -1;
    size_t len;
    as_msg_hdr_t *msgHdrPtr;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;

    worker = ringbuf_register(mgrPtr->captureRingBuf, AUDIO_CAPTURE_WORKER_ID);
    if (worker == NULL) {
        as_log_Err("Failed to regitster cap worker.\n");
        return (void*)-1; 
    }

    pv_recorder_t *recorder = NULL;
    pv_recorder_status_t status = pv_recorder_init(device_index,
                                                   frame_length,
                                                   100,
                                                   true,
                                                   true,
                                                   &recorder);
    if (status != PV_RECORDER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize device with %s.\n",
            pv_recorder_status_to_string(status));
        return (void*)-1;
    }

    const char *selected_device = pv_recorder_get_selected_device(recorder);
    fprintf(stdout, "Selected device: %s.\n", selected_device);

    recordAudDataPtr = malloc(MAX_RECORD_AUDIO_DATA);
    if (recordAudDataPtr == NULL) {
        as_log_Err("Failed to allocat record data buffer.\n");
        pv_recorder_delete(recorder);
        ringbuf_unregister(mgrPtr->captureRingBuf, worker);
        return (void*)-1;
    }
    recordAudDataLen = 0;

    int size = frame_length * 2;
    bool start = false;
    as_log_Debug("Enter as_audio_PvRecordThr...\n");
    while (!mgrPtr->quit) {
        if (mgrPtr->state != AS_SM_RECORDING) {
            if (start) {
                pv_recorder_stop(recorder);
                start = false;
            }
            usleep(10000);
            continue;
        }

        if (!start) {
            as_log_Debug("mgrPtr->state is %d\n", mgrPtr->state);
            status = pv_recorder_start(recorder);
            if (status != PV_RECORDER_STATUS_SUCCESS) {
                fprintf(stderr, "Failed to start device with %s.\n", pv_recorder_status_to_string(status));
                continue;     
            }
            start = true;
        }

        pthread_mutex_lock(&recordMutex);
        status = pv_recorder_read(recorder, recordAudDataPtr + recordAudDataLen);        
        if (status != PV_RECORDER_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to read with %s.\n", pv_recorder_status_to_string(status));
            pthread_mutex_unlock(&recordMutex);
            continue;
        }

        recordAudDataLen += frame_length;
        pthread_mutex_unlock(&recordMutex);
    }

    pv_recorder_stop(recorder);
    pv_recorder_delete(recorder);
    ringbuf_unregister(mgrPtr->captureRingBuf, worker);

    return (void*)0;
}
