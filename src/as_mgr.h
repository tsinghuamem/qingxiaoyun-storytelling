#ifndef _AS_MGR_H_
#define _AS_MGR_H_

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <pthread.h>
#include "ringbuf.h"
#include "as_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKERS                 2
#define AUDIO_CAPTURE_WORKER_ID     0
#define KEY_CAPTURE_WORKER_ID       1
#define AUDIO_RX_MAIN_WORKER_ID     0
#define AUDIO_RX_SUB_WORKER_ID      1

// KEY definition
#define AS_KEY_FOR_MODE             26  // PIN26->GPIO25
#define AS_KEY_FOR_WAKEUP           25  // PIN25->GPIO6. 

#define AS_KEY_FOR_VOLUME_INC       23  // PIN23->GPIO4, volume increase
#define AS_KEY_FOR_VOLUME_DEC       24  // PIN24->GPIO5, volume decrease

#define AUDIO_BUFFER_SIZE           (100 * 1024 * 1024)

#define AS_WAKEUP_TIMER_INTERVAL    100 // 100ms
#define AS_TIMER_DEBOUNCE           20  // 20ms
#define AS_RECORD_TIMER_INTERVAL    5000  // 6s
#define AS_SPEAKER_TIMER_INTERVAL   20000  // 3s


typedef enum {
    AS_SM_INIT = 0,
    AS_SM_AUTH,	// Authorization success.
    AS_SM_IDLE,	// Waiting for operation
    AS_SM_WAKEUP,
    AS_SM_RECORDING,
    AS_SM_TRANSLATE,
    AS_SM_SPEAKING,
    AS_SM_REFUSE,
    AS_SM_DEINIT
}as_sm_t;   // AI story state machine.

// Globle manager
typedef struct {
    pthread_t       audCaptureThr;
    pthread_t       audSpeakThr;
    pthread_t       dataRxTxThr;
    pthread_t       eventMonitorThr;
	pthread_t       hotwordDetectorThr;
    pthread_t       authThr;

    ringbuf_t       *captureRingBuf;    // Audio data or key event which is captured to send.
    uint8_t         *captureBuf;
    ringbuf_t       *receiveRingBuf;    // Rx server's data, contains text and audio.
    uint8_t         *receiveBuf;

    int             mode;           // 0: Inclusive mode; 1: Exclusive mode(专属模式)

    volatile int    state;          // See as_sm_t
    volatile int    auth;           // 0: wait for authorization; 1: authorization has approved.
    volatile int    quit;           // Safe quit flag.
}as_mgr_t;

#ifdef DEBUG
#define as_log_Debug(fmt, ...)      printf("%s:%d:", __FUNCTION__, __LINE__); printf("DEBUG:" fmt, ##__VA_ARGS__)
#define as_log_Info(fmt, ...)       printf("%s:%d:", __FUNCTION__, __LINE__); printf("INFO:" fmt, ##__VA_ARGS__)
#define as_log_Warning(fmt, ...)    printf("%s:%d:", __FUNCTION__, __LINE__); printf("WARN:" fmt, ##__VA_ARGS__)
#define as_log_Err(fmt, ...)        printf("%s:%d:", __FUNCTION__, __LINE__); printf("Error:" fmt, ##__VA_ARGS__)
#define as_log_Fatal(fmt, ...)      printf("%s:%d:", __FUNCTION__, __LINE__); printf("Fatal:" fmt, ##__VA_ARGS__); exit(1)
#else
#define as_log_Debug(fmt, ...)      syslog(LOG_DEBUG, fmt, __VA_ARGS__)
#define as_log_Info(fmt, ...)       syslog(LOG_INFO, fmt, __VA_ARGS__)
#define as_log_Warning(fmt, ...)    syslog(LOG_WARNING, fmt, __VA_ARGS__)
#define as_log_Err(fmt, ...)        syslog(LOG_ERR, fmt, __VA_ARGS__)
#define as_log_Fatal(fmt, ...)      syslog(LOG_EMERG, fmt, __VA_ARGS__); exit(1)    
#endif

uint64_t as_platform_GetTicks();

#ifdef __cplusplus
}
#endif

#endif
