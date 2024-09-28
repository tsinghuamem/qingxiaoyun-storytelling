#ifndef _AS_MSG_DEF_H_
#define _AS_MSG_DEF_H_

#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AS_MSG_TYPE_KEY = 0,
    AS_MSG_TYPE_TEXT,
    AS_MSG_TYPE_AUDIO,
    AS_MSG_TYPE_VIDEO
}as_msg_type_t;

typedef enum {
    AS_KEY_EV_INCREASE_VOLUME,	// Increase volume
    AS_KEY_EV_DECREASE_VOLUME,	// Decrease volume
    AS_KEY_EV_INTERRUPT,		// Interrupt speaker and input sound.
    AS_KEY_EV_CHANGE_MODE		// Change mode.
}as_key_ev_t;

typedef struct {
    uint32_t event;  // See as_key_ev_t
    // No others.
}as_key_body_t;

typedef struct msg_hdr {
    uint32_t type;
    uint32_t len;
    uint8_t  payload[0];
}as_msg_hdr_t;

#ifdef __cplusplus
}
#endif

#endif