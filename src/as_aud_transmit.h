#ifndef _AS_AUD_TRANSMIT_H_
#define _AS_AUD_TRANSMIT_H_

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TEXT_BUF_SIZE       1024
#define MAX_AUDIO_BUF_SIZE      (1024 * 1024 * 2)

#define MAIN_WORKER_NAME        "yunxiaoxin: "
#define SUB_WORKER_NAME         "yunxiaogu: "

#define MAIN_WORKER_VOICE       "zh-CN-XiaoshuangNeural"
#define SUB_WORKER_VOICE        "zh-CN-YunjieNeural"

#define COMMUNICATE_STORY_FILE "../resource/two_speaker_communicate.txt"
#define COMMUNICATE_STORY_KEY  "\xe9\x80\x9a\xe8\xae\xaf"  // 通讯

#define FOOD_STORY_FILE        "../resource/two_speaker_food.txt"
#define FOOD_STORY_KEY         "\xe9\xa3\x9f\xe7\x89\xa9"  // 食物

#define LIGHT_STORY_FILE       "../resource/two_speaker_light.txt"
#define LIGHT_STORY_KEY        "\xe7\x81\xaf\xe5\x85\x89"  // 灯光

#define EDUCATE_STORY_FILE     "../resource/two_speaker_education.txt"
#define EDUCATE_STORY_KEY      "\xe6\x95\x99\xe8\x82\xb2"  // 教育

#define WEATHER_STORY_FILE     "../resource/two_speaker_weather.txt"
#define WEATHER_STORY_KEY      "\xe5\xa4\xa9\xe6\xb0\x94"  // 天气

#define SPORT_STORY_FILE       "../resource/two_speaker_sport.txt"
#define SPORT_STORY_KEY        "\xe8\xbf\x90\xe5\x8a\xa8"  // 运动

#define TRAFFIC_STORY_FILE     "../resource/two_speaker_traffic.txt"
#define TRAFFIC_STORY_KEY      "\xe4\xba\xa4\xe9\x80\x9a"  // 交通

#define ENTERTAIN_STORY_FILE   "../resource/two_speaker_entertain.txt"
#define ENTERTAIN_STORY_KEY    "\xe5\xa8\xb1\xe4\xb9\x90"  // 娱乐

#define MEDICAL_STORY_FILE     "../resource/two_speaker_medical.txt"
#define MEDICAL_STORY_KEY      "\xe5\x8c\xbb\xe7\x96\x97"  // 医疗

#define ARCHITECT_STORY_FILE   "../resource/two_speaker_architect.txt"
#define ARCHITECT_STORY_KEY    "\xe5\xbb\xba\xe7\xad\x91"  // 建筑

#define MAX_STORY_KEY_LEN       16
#define MAX_STORY_NUM           10

typedef struct {
    char key[MAX_STORY_KEY_LEN];
    char fileName[64];
    FILE *fp;
}as_story_info_t;

typedef struct {
    char *dataPtr;
    size_t capability;
    size_t used;
}as_buffer_t;

void *as_audio_TransmitThr(void *arg);


#ifdef __cplusplus
}
#endif

#endif
