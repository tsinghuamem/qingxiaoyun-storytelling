#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#include "as_config.h"
#include "as_mgr.h"

char accessToken[1024];
size_t accessTokenLen = 0;
pthread_mutex_t tokenMutex;

void as_auth_init()
{
    pthread_mutex_init(&tokenMutex, NULL);
}

void as_auth_deinit()
{
    pthread_mutex_destroy(&tokenMutex);
}

static size_t TokenWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;

    // printf("%s", (char *)contents);
    ((char *)userp)[realsize] = '\0';
	
    pthread_mutex_lock(&tokenMutex);
    ((char *)userp)[realsize] = '\0';
    memcpy(userp, contents, realsize);
    accessTokenLen = realsize;
    pthread_mutex_unlock(&tokenMutex);
	
    return realsize;
}

int GetToken(char *tokenBuf, size_t *tokenSize)
{
    if (*tokenSize < strlen(accessToken)) {
        as_log_Err("Input token size is too less");
        return -1;
    }

    pthread_mutex_lock(&tokenMutex);
    memcpy(tokenBuf, accessToken, strlen(accessToken));
    
    pthread_mutex_unlock(&tokenMutex);
}

int IssueTokenFromAzure()
{
    int ret = 0;
    CURL *curl = curl_easy_init();
    CURLcode res;

    curl_easy_setopt(curl, CURLOPT_URL, AZURE_TOKEN_ENPOINT);

    // 请求头设置
    struct curl_slist *headers = NULL;
    //headers = curl_slist_append(headers, "Content-type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Content-Length: 0");
    headers = curl_slist_append(headers, "Ocp-Apim-Subscription-Key: " AZURE_SUBSCRIPTION_KEY);
    //headers = curl_slist_append(headers, "user-agent: curl/7.88.1");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    char data[1] = {"\0"};
	
    // 设置写数据回调函数和指针
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TokenWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, accessToken);
    //curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() error %s\n", curl_easy_strerror(res));
        ret = -1;
        goto out;
    } else {
        long retCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retCode);
        if (retCode != 200) {
            fprintf(stderr, "curl_easy_perform() return %ld\n", retCode);
            ret = -1;
            goto out;
        }
    }
    as_log_Debug("Get token is %s\n", accessToken);
    
out:	
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ret;
}

void *as_auth_IssueThr(void *arg)
{
    int ret = 0;
    as_mgr_t *mgrPtr = (as_mgr_t*)arg;

    const long int refreshTokenDuration = 9 * 60;  // 9min
    struct timeval start, end;
    long int time_taken;

    while (!mgrPtr->quit) {
        if (mgrPtr->state == AS_SM_INIT || mgrPtr->state == AS_SM_REFUSE) {
            ret = IssueTokenFromAzure();
            if (ret != 0) {
                as_log_Warning("Faile to get token from azure. retry...\n");
                usleep(100000);
                continue;
            } else {
                mgrPtr->state = AS_SM_AUTH;
                gettimeofday(&start, NULL);
            }
        } else if (mgrPtr->state == AS_SM_AUTH
            || mgrPtr->state == AS_SM_RECORDING
            || mgrPtr->state == AS_SM_SPEAKING) {
            gettimeofday(&end, NULL);
            time_taken = end.tv_sec - start.tv_sec;
            if (time_taken >= refreshTokenDuration) {
                as_log_Debug("Reach 9min, refresh the token...\n");
                ret = IssueTokenFromAzure();
                if (ret != 0) {
                    as_log_Warning("Faile to refresh token from azure. retry...\n");
                    usleep(10000);
                    continue;					
                }
                gettimeofday(&start, NULL);
            }
        } else {
            sleep(1);
        }
    }

    return (void *)0;
}
