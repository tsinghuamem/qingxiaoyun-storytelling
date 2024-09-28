#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <time.h>

#include "MultiTimer.h"
#include "as_mgr.h"
#include "ringbuf.h"
#include "as_msg_def.h"
#include "as_aud_transmit.h"
#include "as_authorization.h"
#include "as_aud_capture.h"

as_buffer_t sttBuf;
as_buffer_t ttsBuf;

static as_story_info_t story_info[] = {
    {COMMUNICATE_STORY_KEY, COMMUNICATE_STORY_FILE, NULL},
    {FOOD_STORY_KEY, FOOD_STORY_FILE, NULL},
    {LIGHT_STORY_KEY, LIGHT_STORY_FILE, NULL},
    {EDUCATE_STORY_KEY, EDUCATE_STORY_FILE, NULL},
    {WEATHER_STORY_KEY, WEATHER_STORY_FILE, NULL},
    {SPORT_STORY_KEY, SPORT_STORY_FILE, NULL},
    {TRAFFIC_STORY_KEY, TRAFFIC_STORY_FILE, NULL},
    {ENTERTAIN_STORY_KEY, ENTERTAIN_STORY_FILE, NULL},
    {MEDICAL_STORY_KEY, MEDICAL_STORY_FILE, NULL},
    {ARCHITECT_STORY_KEY, ARCHITECT_STORY_FILE, NULL}
};

static int story_content_platform_init()
{
    int i;
    int num = sizeof(story_info) / sizeof(story_info[0]);

    for (i = 0; i < num; i++) {
        as_log_Debug("Key: %s, File name: %s\n",
                story_info[i].key, story_info[i].fileName);
        story_info[i].fp = fopen(story_info[i].fileName, "rb");
        if (story_info[i].fp == NULL) {
            as_log_Err("Failed to open two_speakers.txt\n");
            break;
        }
    }

    if (i < num) {
        num = i;
        for (i = 0; i < num; i++) {
            fclose(story_info[i].fp);
        }

        return -1;
    }

    return 0;
}

static void story_content_platform_deinit()
{
    int i;
    int num = sizeof(story_info) / sizeof(story_info[0]);

    for (i = 0; i < num; i++) {
        fclose(story_info[i].fp);
    }
}

static FILE *story_fp_get(char *text)
{
    int i;
    int num = sizeof(story_info) / sizeof(story_info[0]);

    for (i = 0; i < num; i++) {
        char *found = strstr(text, story_info[i].key);
        if (found != NULL) {
            break;
        }
    }

    if (i == num) {
        // Not found. get a randon fp
        srand(time(0));
        i = rand() % num;
    }

    return story_info[i].fp;
}

size_t TtsWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
     size_t realsize = size * nmemb;
    as_buffer_t *buf = (as_buffer_t*)userp;

    if (realsize > (buf->capability - buf->used)) {
        as_log_Warning("Reduce the text contents\n");
        realsize = buf->capability - buf->used;
    }

    //as_log_Debug("Receive data size is (%llu)\n", realsize);
	memcpy(buf->dataPtr + buf->used, contents, realsize);
    buf->used += realsize;
    //as_log_Debug("Get audio size(%llu)\n", buf->used);

    return realsize;
}

int as_azure_TextToSpeech(CURL *curl, char *voice, char *text_data, size_t text_size,
        char *audio_data, size_t *audio_size)
{
    CURLcode res;
    as_buffer_t buf;

    as_log_Debug("as_azure_TextToSpeech Start...\n");
    curl_easy_setopt(curl, CURLOPT_URL, AZURE_TTS_ENDPOINT);

    char tmpBuf[1024];
    pthread_mutex_lock(&tokenMutex);
    sprintf(tmpBuf, "Authorization: Bearer %s", accessToken);
    pthread_mutex_unlock(&tokenMutex);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/ssml+xml");
    headers = curl_slist_append(headers, "X-Microsoft-OutputFormat: raw-24khz-16bit-mono-pcm");
    headers = curl_slist_append(headers, "Ocp-Apim-Subscription-Key: " AZURE_SUBSCRIPTION_KEY);
    //headers = curl_slist_append(headers, "Host: eastus.tts.speech.microsoft.com");
    headers = curl_slist_append(headers, tmpBuf);
    headers = curl_slist_append(headers, "Connection: Keep-Alive");
    headers = curl_slist_append(headers, "User-Agent: AI Story");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5000);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5000);
    as_log_Debug("as_azure_TextToSpeech set header...\n");
    sprintf(tmpBuf, "<speak version='1.0' xml:lang='en-US'><voice xml:lang='zh-CN' xml:gender='Male' \
        name='%s'> %s </voice></speak>",
        voice, text_data);  // zh-cn-YunxiNeural
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, tmpBuf);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(tmpBuf));
    as_log_Debug("as_azure_TextToSpeech set field, len(%lu):%s\n", strlen(tmpBuf), tmpBuf);

    buf.dataPtr = audio_data;
    buf.capability = *audio_size;
    buf.used = 0;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TtsWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    as_log_Debug("as_azure_TextToSpeech perform curl\n");

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() errror: %s\n", curl_easy_strerror(res));
	} else {
		long retCode;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retCode);
		if (retCode != 200) {
            as_log_Err("curl_easy_perform get response with failure status. return %ld\n", retCode);
            curl_slist_free_all(headers);
            return -1;
        }
	}

    //as_log_Debug("STT Receive data size is %llu\n", buf.used);
    *audio_size = buf.used;

    // Release curl resources.
    curl_slist_free_all(headers);
    return 0;
}

size_t SttWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
     size_t realsize = size * nmemb;
    as_buffer_t *buf = (as_buffer_t*)userp;

    if (realsize > (buf->capability - buf->used)) {
        as_log_Warning("Reduce the text contents\n");
        realsize = buf->capability - buf->used;
    }

    char *subStr = strstr((char*)contents, "\"DisplayText\"");
    if (subStr != NULL) {
        subStr[strlen(subStr)-2] = '\0';
        strcpy(buf->dataPtr + buf->used, subStr + 15);
        buf->used += strlen(subStr + 15);
    }

    //as_log_Debug("Get text(%llu): %s\n", buf->used, buf->dataPtr);

    return realsize;
}

int as_azure_SpeechToText(CURL *curl, char *audio_data, size_t audio_size, char *text_data, size_t *text_size)
{
    CURLcode res;
    as_buffer_t buf;

    curl_easy_setopt(curl, CURLOPT_URL, AZURE_STT_ENDPOINT);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: audio/wav; codec=audio/pcm; samplerate=22050");
    headers = curl_slist_append(headers, "Ocp-Apim-Subscription-Key: " AZURE_SUBSCRIPTION_KEY);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, audio_size);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, audio_data);

    buf.dataPtr = text_data;
    buf.capability = *text_size;
    buf.used = 0;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SttWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() errror: %s\n", curl_easy_strerror(res));
	} else {
		long retCode;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retCode);
		if (retCode != 200) {
            as_log_Err("curl_easy_perform get response with failure status. return %ld\n", retCode);
            curl_slist_free_all(headers);
            return -1;
        }
	}
    as_log_Debug("STT Receive data size is %llu\n", buf.used);
    *text_size = buf.used;

    // Release curl resources.
    curl_slist_free_all(headers);
    return 0;
}

void as_speaker_idle_TimerHandler(MultiTimer *timer, void *userData)
{
    as_mgr_t *mgrPtr = (as_mgr_t*)userData;
    as_log_Debug("speaker idle timer fired at %lu ms\n", as_platform_GetTicks());

    mgrPtr->state = AS_SM_IDLE;
}

void *as_audio_TransmitThr(void *arg)
{
    int ret = 0;
    size_t len, off;
    ringbuf_worker_t *mainWorker = NULL;
    ringbuf_worker_t *subWorker = NULL;
    as_msg_hdr_t *msgHdrPtr;
    FILE *fp = NULL;
    MultiTimer speakerTimer;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;

    story_content_platform_init();    

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        as_log_Err("curl_easy_init error.\n");
        return (void*)-1;
    }

    sttBuf.dataPtr = malloc(MAX_TEXT_BUF_SIZE);
    if (sttBuf.dataPtr == NULL) {
        as_log_Err("Failed to allocate memory for stt.\n");
        goto out;    
    }
    sttBuf.capability = MAX_TEXT_BUF_SIZE;
    sttBuf.used = 0;

    ttsBuf.dataPtr = malloc(MAX_AUDIO_BUF_SIZE);
    if (ttsBuf.dataPtr == NULL) {
        as_log_Err("Failed to allocate memory for tts.\n");
        goto out;    
    }
    ttsBuf.capability = MAX_AUDIO_BUF_SIZE;
    ttsBuf.used = 0;    

    mainWorker = ringbuf_register(mgrPtr->receiveRingBuf, AUDIO_RX_MAIN_WORKER_ID);
    if (mainWorker == NULL) {
        as_log_Err("Failed to register main worker.\n");
        goto out; 
    }

    subWorker = ringbuf_register(mgrPtr->receiveRingBuf, AUDIO_RX_SUB_WORKER_ID);
    if (subWorker == NULL) {
        as_log_Err("Failed to register sub worker.\n");
        goto out; 
    }

    while (!mgrPtr->quit) {
        if (mgrPtr->state == AS_SM_TRANSLATE) {
            pthread_mutex_lock(&recordMutex);
            // Send the audio data to AI Agent.
            size_t rlen = sttBuf.capability - sttBuf.used;
            as_azure_SpeechToText(curl, (char *)recordAudDataPtr, recordAudDataLen * 2,
                    sttBuf.dataPtr + sttBuf.used, &rlen);
            sttBuf.used += rlen;
            pthread_mutex_unlock(&recordMutex);
            as_log_Debug("Text: %s\n", sttBuf.dataPtr);

            
            // Send the text to NLP engine and get the result.
            fp = story_fp_get(sttBuf.dataPtr);

            // Send the text result to TTS engine.
            // 文本需要分配避免音频数据量过大
            int cnt1 = 0;
            int cnt2 = 0;
            fseek(fp, 0, SEEK_SET);
            char txtBuf[512];

            while(fgets(txtBuf, sizeof(txtBuf), fp) != NULL) {
                as_log_Debug("Read: %s\n", txtBuf);
                ttsBuf.used = 0; // Reset the tts buffer
                rlen = ttsBuf.capability;

                if (strncmp(txtBuf, MAIN_WORKER_NAME, strlen(MAIN_WORKER_NAME)) == 0) {
                    ret = as_azure_TextToSpeech(curl, MAIN_WORKER_VOICE, 
                            txtBuf + strlen(MAIN_WORKER_NAME),
                            strlen(txtBuf), ttsBuf.dataPtr, &rlen);
                    if (ret != 0) {
                        as_log_Err("Failed to process tts\n");
                        continue;
                    }
                    ttsBuf.used += rlen;
                    
                    // Audio spreaker
                    off = ringbuf_acquire(mgrPtr->receiveRingBuf, mainWorker,
                            ttsBuf.used + sizeof(as_msg_hdr_t));
                    //as_log_Debug("Main worker(off:%llu, len:%llu)...\n",
                    //        off, ttsBuf.used + sizeof(as_msg_hdr_t));
                    if (off > AUDIO_BUFFER_SIZE) {
                        as_log_Err("Ring buffer is full. Failed to get offset.\n");
                        usleep(10000);
                        continue;
                    }
                    
                    msgHdrPtr = (as_msg_hdr_t*)(&(mgrPtr->receiveBuf[off]));
                    msgHdrPtr->type = AS_MSG_TYPE_AUDIO;
                    msgHdrPtr->len = ttsBuf.used;
                    memcpy(msgHdrPtr->payload, ttsBuf.dataPtr, ttsBuf.used);
                    cnt1++;
                    ringbuf_produce(mgrPtr->receiveRingBuf, mainWorker);
                }
                else if (strncmp(txtBuf, SUB_WORKER_NAME, strlen(SUB_WORKER_NAME)) == 0) {
                    ret = as_azure_TextToSpeech(curl, SUB_WORKER_VOICE, 
                            txtBuf + strlen(SUB_WORKER_NAME), strlen(txtBuf),
                            ttsBuf.dataPtr, &rlen);
                    if (ret != 0) {
                        as_log_Err("Failed to process tts\n");
                        continue;
                    }
                    ttsBuf.used += rlen;
                    
                    // Audio spreaker
                    off = ringbuf_acquire(mgrPtr->receiveRingBuf, subWorker,
                            ttsBuf.used + sizeof(as_msg_hdr_t));
                    //as_log_Debug("Sub worker(off:%llu, len:%llu)...\n",
                    //        off, ttsBuf.used + sizeof(as_msg_hdr_t));
                    if (off > AUDIO_BUFFER_SIZE) {
                        as_log_Err("Ring buffer is full. Failed to get offset.\n");
                        usleep(10000);
                        continue;
                    }
                    
                    msgHdrPtr = (as_msg_hdr_t*)(&(mgrPtr->receiveBuf[off]));
                    msgHdrPtr->type = AS_MSG_TYPE_AUDIO;
                    msgHdrPtr->len = ttsBuf.used;
                    memcpy(msgHdrPtr->payload, ttsBuf.dataPtr, ttsBuf.used);
                    cnt2++;
                    ringbuf_produce(mgrPtr->receiveRingBuf, subWorker);
                }
                else {
                    as_log_Err("Unknown txt worker.\n");
                    continue;
                }

                //if (cnt1 > 3 && cnt2 > 3) {
                //    mgrPtr->state = AS_SM_SPEAKING;
                //}
                mgrPtr->state = AS_SM_SPEAKING;
                //usleep(5000); // Let it slow
            }

            
            // Start the timer for recording.
            multiTimerStart(&speakerTimer, AS_SPEAKER_TIMER_INTERVAL,
                as_speaker_idle_TimerHandler, (void*)mgrPtr);
        }
        else {
            usleep(1000);
        }
    }

out:   
    if (mainWorker != NULL) {
        ringbuf_unregister(mgrPtr->receiveRingBuf, mainWorker);
    }

    if (subWorker != NULL) {
        ringbuf_unregister(mgrPtr->receiveRingBuf, subWorker);
    }
    
    if (sttBuf.dataPtr != NULL) {
        free(sttBuf.dataPtr);
    }
    
    if (ttsBuf.dataPtr != NULL) {
        free(ttsBuf.dataPtr);
    }
        
    if (curl != NULL) {        
        curl_easy_cleanup(curl);
    }

    multiTimerStop(&speakerTimer);
    story_content_platform_deinit();

    return (void*)0;
}
