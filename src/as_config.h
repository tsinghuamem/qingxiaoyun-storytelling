#ifndef _AS_CONFIG_H_
#define _AS_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define USING_PV_VOICE    1

#define PORCUPINE_MODEL_PATH    "../resource/porcupine_params_zh.pv"
#define PORCUPINE_KEYWORD_PATH  "../resource/qingxiaoyun_zh_raspberry-pi_v3_0_0.ppn"
#define PORCUPINE_LIB_PATH      "../lib/libpv_porcupine.so"
#define PV_ACCESS_KEY           "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// Azure info
#define AZURE_TTS_ENDPOINT  "https://eastasia.tts.speech.microsoft.com/cognitiveservices/v1"
#define AZURE_STT_ENDPOINT  "https://eastasia.stt.speech.microsoft.com/speech/recognition/conversation/cognitiveservices/v1?language=zh-CN"
#define AZURE_TOKEN_ENPOINT "https://eastasia.api.cognitive.microsoft.com/sts/v1.0/issueToken"
#define AZURE_SUBSCRIPTION_KEY  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define AZURE_GET_TOKEN_CMD     "curl -X POST \
                    \"https://eastasia.api.cognitive.microsoft.com/sts/v1.0/issueToken\" \
                    -H \"Content-type: application/x-www-form-urlencoded\" \
                    -H \"Content-Length: 0\" \
                    -H \"Ocp-Apim-Subscription-Key: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\""

//#define AUDIO_CAPTURE_DEV   "/dev/snd/pcmC0D0c"
#define AUDIO_CAPTURE_DEV   "default"

//#define AUDIO_MIXER_DEV     "default"
#define AUDIO_MIXER_DEV     "hw:0"
#define AUDIO_MIXER_SELEM_NAME  "Playback"

#define SPEAKER_DEVICE_ID           0

#define MIC4WAKEUP_DEVICE_ID        1
#define MIC4RECORD_DEVICE_ID        2   // Device2: echo cancelled with Built-in Audio Stereo.

#ifdef __cplusplus
}
#endif

#endif

