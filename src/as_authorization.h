#ifndef _AS_AUTHORIZATION_H_
#define _AS_AUTHORIZATION_H_

#ifdef __cplusplus
extern "C" {
#endif

extern char accessToken[1024];
extern size_t accessTokenLen;
extern pthread_mutex_t tokenMutex;

void as_auth_init();
void as_auth_deinit();
void *as_auth_IssueThr(void *arg);
int GetToken(char *tokenBuf, size_t *tokenSize);

#ifdef __cplusplus
}
#endif

#endif

