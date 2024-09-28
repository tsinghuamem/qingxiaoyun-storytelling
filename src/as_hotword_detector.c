#include <dlfcn.h>
#include "pv_porcupine.h"
#include "pv_recorder.h"

#include "as_mgr.h"

static void *open_dl(const char *dl_path)
{
    return dlopen(dl_path, RTLD_NOW);
}

static void *load_symbol(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

static void close_dl(void *handle)
{
    dlclose(handle);
}

static void print_dl_error(const char *message)
{
    fprintf(stderr, "%s with '%s'.\n", message, dlerror());
}

void print_error_message(char **message_stack, int32_t message_stack_depth) {
    for (int32_t i = 0; i < message_stack_depth; i++) {
        fprintf(stderr, "  [%d] %s\n", i, message_stack[i]);
    }
}

void *as_hotword_DetectorThr(void *arg)
{
    int ret = 0;
    const char *library_path = PORCUPINE_LIB_PATH;
    const char *model_path = PORCUPINE_MODEL_PATH;
    const char *keyword_path = PORCUPINE_KEYWORD_PATH;
    float sensitivity = 0.5f;
    const char *access_key = PV_ACCESS_KEY;
    int32_t device_index = MIC4WAKEUP_DEVICE_ID;

    as_mgr_t *mgrPtr = (as_mgr_t*)arg;

    void *porcupine_library = open_dl(library_path);
    if (!porcupine_library) {
        as_log_Err("failed to open library.\n");
        return (void*)-1;
    }

    const char *(*pv_status_to_string_func)(pv_status_t) = load_symbol(porcupine_library, "pv_status_to_string");
    if (!pv_status_to_string_func) {
        as_log_Err("failed to load 'pv_status_to_string'\n");
        return (void*)-1;
    }

    int32_t (*pv_sample_rate_func)() = load_symbol(porcupine_library, "pv_sample_rate");
    if (!pv_sample_rate_func) {
        as_log_Err("failed to load 'pv_sample_rate'\n");
        return (void*)-1;
    }

    pv_status_t (*pv_porcupine_init_func)(const char *, const char *, int32_t, const char *const *, const float *, pv_porcupine_t **) =
            load_symbol(porcupine_library, "pv_porcupine_init");
    if (!pv_porcupine_init_func) {
        as_log_Err("failed to load 'pv_porcupine_init'\n");
        return (void*)-1;
    }

    void (*pv_porcupine_delete_func)(pv_porcupine_t *) = load_symbol(porcupine_library, "pv_porcupine_delete");
    if (!pv_porcupine_delete_func) {
        as_log_Err("failed to load 'pv_porcupine_delete'\n");
        return (void*)-1;
    }

    pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *, int32_t *) = load_symbol(porcupine_library, "pv_porcupine_process");
    if (!pv_porcupine_process_func) {
        as_log_Err("failed to load 'pv_porcupine_process'\n");
        return (void*)-1;
    }

    int32_t (*pv_porcupine_frame_length_func)() = load_symbol(porcupine_library, "pv_porcupine_frame_length");
    if (!pv_porcupine_frame_length_func) {
        as_log_Err("failed to load 'pv_porcupine_frame_length'\n");
        return (void*)-1;
    }

    const char *(*pv_porcupine_version_func)() = load_symbol(porcupine_library, "pv_porcupine_version");
    if (!pv_porcupine_version_func) {
        as_log_Err("failed to load 'pv_porcupine_version'\n");
        return (void*)-1;
    }

    pv_status_t (*pv_get_error_stack_func)(char ***, int32_t *) = load_symbol(porcupine_library, "pv_get_error_stack");
    if (!pv_get_error_stack_func) {
        as_log_Err("failed to load 'pv_get_error_stack_func'\n");
        return (void*)-1;
    }

    void (*pv_free_error_stack_func)(char **) = load_symbol(porcupine_library, "pv_free_error_stack");
    if (!pv_free_error_stack_func) {
        as_log_Err("failed to load 'pv_free_error_stack_func'\n");
        return (void*)-1;
    }

    char **message_stack = NULL;
    int32_t message_stack_depth = 0;
    pv_status_t error_status = PV_STATUS_RUNTIME_ERROR;

    pv_porcupine_t *porcupine = NULL;
    pv_status_t porcupine_status = pv_porcupine_init_func(access_key, model_path, 1, &keyword_path, &sensitivity, &porcupine);
    if (porcupine_status != PV_STATUS_SUCCESS) {
        fprintf(stderr, "'pv_porcupine_init' failed with '%s'", pv_status_to_string_func(porcupine_status));
        error_status = pv_get_error_stack_func(&message_stack, &message_stack_depth);
        if (error_status != PV_STATUS_SUCCESS) {
            fprintf(stderr, ".\nUnable to get Porcupine error state with '%s'.\n", pv_status_to_string_func(error_status));
            return (void*)-1;
        }

        if (message_stack_depth > 0) {
            fprintf(stderr, ":\n");
            print_error_message(message_stack, message_stack_depth);
            pv_free_error_stack_func(message_stack);
        } else {
            fprintf(stderr, ".\n");
        }

        return (void*)-1;
    }

    fprintf(stdout, "V%s\n\n", pv_porcupine_version_func());

    const int32_t frame_length = pv_porcupine_frame_length_func();
    pv_recorder_t *recorder = NULL;
    pv_recorder_status_t recorder_status = pv_recorder_init(device_index, frame_length, 100, true, true, &recorder);
    if (recorder_status != PV_RECORDER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize device with %s.\n", pv_recorder_status_to_string(recorder_status));
        return (void*)-1;
    }

    const char *selected_device = pv_recorder_get_selected_device(recorder);
    fprintf(stdout, "Selected device: %s.\n", selected_device);

	fprintf(stdout, "frame_len = %d\n\n", frame_length);

    int16_t *pcm = malloc(frame_length * sizeof(int16_t));
    if (!pcm) {
        fprintf(stderr, "Failed to allocate pcm memory.\n");
        return (void*)-1;
    }

    bool start = false;
    while (!mgrPtr->quit) {	
        //if (mgrPtr->state == AS_SM_IDLE || mgrPtr->state == AS_SM_SPEAKING) {
        if (mgrPtr->state == AS_SM_IDLE) {
            if (!start) {
                fprintf(stdout, "Start recording...\n");
                recorder_status = pv_recorder_start(recorder);
                if (recorder_status != PV_RECORDER_STATUS_SUCCESS) {
                    fprintf(stderr, "Failed to start device with %s.\n", pv_recorder_status_to_string(recorder_status));
                    continue;
                }
                start = true;
            }
            
            recorder_status = pv_recorder_read(recorder, pcm);
            if (recorder_status != PV_RECORDER_STATUS_SUCCESS) {
                fprintf(stderr, "Failed to read with %s.\n", pv_recorder_status_to_string(recorder_status));
                continue;
            }

            int32_t keyword_index = -1;
            porcupine_status = pv_porcupine_process_func(porcupine, pcm, &keyword_index);
            if (porcupine_status != PV_STATUS_SUCCESS) {
                fprintf(stderr, "'pv_porcupine_process' failed with '%s'", pv_status_to_string_func(porcupine_status));
                error_status = pv_get_error_stack_func(&message_stack, &message_stack_depth);
                if (error_status != PV_STATUS_SUCCESS) {
                    fprintf(stderr, ".\nUnable to get Porcupine error state with '%s'.\n", pv_status_to_string_func(error_status));
                    return (void*)-1;
                }

                if (message_stack_depth > 0) {
                    fprintf(stderr, ":\n");
                    print_error_message(message_stack, message_stack_depth);
                    pv_free_error_stack_func(message_stack);
                } else {
                    fprintf(stderr, ".\n");
                }
                return (void*)-1;
            }
			
            if (keyword_index != -1) {
                // Wake up, enter recording...
                mgrPtr->state = AS_SM_WAKEUP;
                fprintf(stdout, "keyword detected\n");
                fflush(stdout);
            }
        } 
        else {
            if (start) {
                pv_recorder_stop(recorder);
                start = false;
            }
            
            usleep(1000);
        }
    }

    fprintf(stdout, "as_hotword_DetectorThr quit\n\n");
    recorder_status = pv_recorder_stop(recorder);
    if (recorder_status != PV_RECORDER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to stop device with %s.\n", pv_recorder_status_to_string(recorder_status));
        return (void*)-1;
    }

    free(pcm);
    pv_recorder_delete(recorder);
    pv_porcupine_delete_func(porcupine);
    close_dl(porcupine_library);

    return (void*)0;		
}

