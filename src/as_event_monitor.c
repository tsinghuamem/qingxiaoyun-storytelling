#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "MultiTimer.h"
#include "as_mgr.h"
#include <wiringPi.h>
#include "as_event_monitor.h"
#include "as_aud_speak.h"

static bool isWakeupKeyTimerRun = false;
static bool isModeKeyTimerRun = false;
static bool isVolIncKeyTimerRun = false;
static bool isVolDecKeyTimerRun = false;


void as_event_WakeupKeyTimerHandler(MultiTimer *timer, void *userData)
{
    as_mgr_t *mgrPtr = (as_mgr_t*)userData;
    as_log_Debug("WakeupKeyTimer fired at %lu ms\n", as_platform_GetTicks());

    // Get the state of pin AS_KEY_FOR_WAKEUP
    int value = digitalRead(AS_KEY_FOR_WAKEUP);
    if (LOW == value) {
        mgrPtr->state = AS_SM_RECORDING;

        // Restart timer
        multiTimerStart(timer, AS_WAKEUP_TIMER_INTERVAL, as_event_WakeupKeyTimerHandler, NULL);
    } else {
        mgrPtr->state = AS_SM_SPEAKING;
        isWakeupKeyTimerRun = false;
    }
}

void as_event_modeKeyTimerHandler(MultiTimer *timer, void *userData)
{
    as_mgr_t *mgrPtr = (as_mgr_t*)userData;
    as_log_Debug("ModeKeyTimer fired at %lu ms\n", as_platform_GetTicks());

    // Get the state of pin AS_KEY_FOR_MODE
    int value = digitalRead(AS_KEY_FOR_MODE);
    if (LOW == value) {
        mgrPtr->mode = !(mgrPtr->mode);
    } else {
        isModeKeyTimerRun = false;
    }
}

void as_event_volumeIncKeyTimerHandler(MultiTimer *timer, void *userData)
{
    as_log_Debug("VolumeIncKeyTimer fired at %lu ms\n", as_platform_GetTicks());

    // Get the state of pin AS_KEY_FOR_VOLUME_INC
    int value = digitalRead(AS_KEY_FOR_VOLUME_INC);
    if (LOW == value) {
        // Call volume increase function to add 10db.
        as_mixer_IncreaseSpeakerVolume();
    } else {
        isVolIncKeyTimerRun = false;
    }
}

void as_event_volumeDecKeyTimerHandler(MultiTimer *timer, void *userData)
{
    as_log_Debug("VolumeDecKeyTimer fired at %lu ms\n", as_platform_GetTicks());

    // Get the state of pin AS_KEY_FOR_VOLUME_DEC
    int value = digitalRead(AS_KEY_FOR_VOLUME_DEC);
    if (LOW == value) {
        // Call volume decrease function to subtract 10db.
        as_mixer_DecreaseSpeakerVolume();
    } else {
        isVolDecKeyTimerRun = false;
    }
}

void *as_event_MonitorThr(void *arg)
{
    int ret = 0;
    int value;
    MultiTimer wakeupKeyTimer;
    MultiTimer modeKeyTimer;
    MultiTimer volumeIncTimer;
    MultiTimer volumeDecTimer;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;

    ret = multiTimerInstall(as_platform_GetTicks);
    if (ret != 0) {
        as_log_Err("Failed to install multi-timer.\n");
        return (void*)-1;
    }

    while (!mgrPtr->quit) {
        multiTimerYield();

        if (!isModeKeyTimerRun) {
            value = digitalRead(AS_KEY_FOR_MODE);
            if (LOW == value) {
                // Start the timer for debounce.
                multiTimerStart(&modeKeyTimer, AS_TIMER_DEBOUNCE,
                    as_event_modeKeyTimerHandler, (void*)mgrPtr);
            }
            isModeKeyTimerRun = true;
        }

        if (!isWakeupKeyTimerRun) {
            value = digitalRead(AS_KEY_FOR_WAKEUP);
            if (LOW == value) {
                // Start the timer for debounce.
                multiTimerStart(&wakeupKeyTimer, AS_TIMER_DEBOUNCE,
                    as_event_WakeupKeyTimerHandler, (void*)mgrPtr);
            }
            isWakeupKeyTimerRun = true;            
        }

        if (!isVolIncKeyTimerRun) {
            value = digitalRead(AS_KEY_FOR_VOLUME_INC);
            if (LOW == value) {
                // Start the timer for debounce.
                multiTimerStart(&volumeIncTimer, AS_TIMER_DEBOUNCE,
                    as_event_volumeIncKeyTimerHandler, NULL);
            }
            isVolIncKeyTimerRun = true;            
        }

        if (!isVolDecKeyTimerRun) {
            value = digitalRead(AS_KEY_FOR_VOLUME_DEC);
            if (LOW == value) {
                // Start the timer for debounce.
                multiTimerStart(&volumeDecTimer, AS_TIMER_DEBOUNCE,
                    as_event_volumeDecKeyTimerHandler, NULL);
            }
            isVolDecKeyTimerRun = true;           
        }
        
        usleep(10000);
    }

out:
    if (isModeKeyTimerRun) {
        multiTimerStop(&modeKeyTimer);
    }
    
    if (isWakeupKeyTimerRun) {
        multiTimerStop(&wakeupKeyTimer);
    }
    
    if (isVolIncKeyTimerRun) {
        multiTimerStop(&volumeIncTimer);
    }
    
    if (isVolDecKeyTimerRun) {
        multiTimerStop(&volumeDecTimer);
    }

    return (void*)0;
}