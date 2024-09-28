#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <pthread.h>
#include <wiringPi.h> // Include WiringPi library!
#include <curl/curl.h>

#include "as_mgr.h"
#include "as_aud_capture.h"
#include "as_aud_speak.h"
#include "as_aud_transmit.h"
#include "as_event_monitor.h"
#include "as_hotword_detector.h"
#include "as_authorization.h"

static as_mgr_t *glbMgrPtr = NULL;

void handle_signal(int sig)
{
    printf("Caught signal %d\n", sig);

    if (glbMgrPtr != NULL) {
        glbMgrPtr->quit = 1;
    }
}

int as_platform_Init()
{
    openlog("AIStory", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    // uses BCM numbering of the GPIOs and directly accesses the GPIO registers.
    wiringPiSetupGpio();

    // pin mode ..(INPUT, OUTPUT, PWM_OUTPUT, GPIO_CLOCK)
    pinMode(AS_KEY_FOR_MODE, INPUT);
    pinMode(AS_KEY_FOR_WAKEUP, INPUT);
    pinMode(AS_KEY_FOR_VOLUME_INC, INPUT);
    pinMode(AS_KEY_FOR_VOLUME_DEC, INPUT);

    // pull up/down mode (PUD_OFF, PUD_UP, PUD_DOWN) => down
    pullUpDnControl(AS_KEY_FOR_MODE, PUD_UP);
    pullUpDnControl(AS_KEY_FOR_WAKEUP, PUD_UP);
    pullUpDnControl(AS_KEY_FOR_VOLUME_INC, PUD_UP);
    pullUpDnControl(AS_KEY_FOR_VOLUME_DEC, PUD_UP);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    struct sigaction sa;
    sa.sa_handler = &handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        as_log_Err("Failed to regitster sigaction.\n");
        return -1;
    }

    return 0;
}

int as_platform_Deinit()
{
    curl_global_cleanup();

    return 0;
}

// Platform-specific function to get current ticks (milliseconds)
uint64_t as_platform_GetTicks()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000LL + now.tv_usec / 1000;
}

int main(int argc, char *argv[])
{
    int ret;
    
    as_platform_Init();

    glbMgrPtr = (as_mgr_t*)calloc(1, sizeof(as_mgr_t));
    if (glbMgrPtr == NULL) {
        as_log_Err("Failed to alloc memory for mgr.\n");
        exit(1);
    }

    // Initilize global mananger.
    glbMgrPtr->mode = 0;
    glbMgrPtr->state = AS_SM_INIT;
    glbMgrPtr->quit = 0;

    // Allocate memory for ring buffer.
    size_t ringbuf_obj_size;
    ringbuf_get_sizes(MAX_WORKERS, &ringbuf_obj_size, NULL);
    glbMgrPtr->captureRingBuf = (ringbuf_t*)calloc(1, ringbuf_obj_size);
    if (glbMgrPtr->captureRingBuf == NULL) {
        as_log_Err("Failed to alloc memory for capture ring buffer.\n");
        exit(1);
    }

    glbMgrPtr->captureBuf = malloc(AUDIO_BUFFER_SIZE);
    if (glbMgrPtr->captureBuf == NULL) {
        as_log_Err("Failed to alloc memory for capture buffer.\n");
        exit(1);
    }

    ringbuf_setup(glbMgrPtr->captureRingBuf, MAX_WORKERS, AUDIO_BUFFER_SIZE);

    ringbuf_get_sizes(MAX_WORKERS, &ringbuf_obj_size, NULL);
    glbMgrPtr->receiveRingBuf = (ringbuf_t*)calloc(1, ringbuf_obj_size);
    if (glbMgrPtr->receiveRingBuf == NULL) {
        as_log_Err("Failed to alloc memory for rx ring buffer.\n");
        exit(1);
    }

    glbMgrPtr->receiveBuf = malloc(AUDIO_BUFFER_SIZE);
    if (glbMgrPtr->receiveBuf == NULL) {
        as_log_Err("Failed to alloc memory for rx buffer.\n");
        exit(1);
    }

    ringbuf_setup(glbMgrPtr->receiveRingBuf, MAX_WORKERS, AUDIO_BUFFER_SIZE);

#ifdef USING_PV_VOICE
    // Start audio capture thread.
    ret = pthread_create(&glbMgrPtr->audCaptureThr, NULL, as_audio_PvRecordThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create audio capture thread.\n");
        exit(1); 
    }

    // Start audio speak tdhread.
    ret = pthread_create(&glbMgrPtr->audSpeakThr, NULL, as_audio_PvSpeakThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create audio speak thread.\n");
        exit(1); 
    }   
#else   // Get conflit with hotword detector
    // Start audio capture thread.
    ret = pthread_create(&glbMgrPtr->audCaptureThr, NULL, as_audio_CaptureThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create audio capture thread.\n");
        exit(1); 
    }

    // Start audio speak tdhread.
    ret = pthread_create(&glbMgrPtr->audSpeakThr, NULL, as_audio_SpeakThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create audio speak thread.\n");
        exit(1); 
    }    
#endif
    // Start protocol handle thread.
    ret = pthread_create(&glbMgrPtr->dataRxTxThr, NULL, as_audio_TransmitThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create audio transmit thread.\n");
        exit(1); 
    }      

    // Start protocol handle thread.
    ret = pthread_create(&glbMgrPtr->eventMonitorThr, NULL, as_event_MonitorThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create event monitor thread.\n");
        exit(1); 
    }

    ret = pthread_create(&glbMgrPtr->hotwordDetectorThr, NULL, as_hotword_DetectorThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create hotword detector thread.\n");
        exit(1); 
    }

    ret = pthread_create(&glbMgrPtr->authThr, NULL, as_auth_IssueThr, glbMgrPtr);
    if (ret != 0) {
        as_log_Err("Failed to create authorization thread.\n");
        exit(1); 
    }
    
    while (!glbMgrPtr->quit) {
        // feed watchdog.
        
        usleep(100000);
    }
    as_platform_Deinit();
    
    return 0;
}
