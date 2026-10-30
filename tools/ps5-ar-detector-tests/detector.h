#ifndef PS5_AR_DETECTOR_H
#define PS5_AR_DETECTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DETECT_UNKNOWN = 0,
    DETECT_NO_GAME = 1,
    DETECT_GAME_ACTIVE = 2,
    DETECT_GAME_BACKGROUND = 3,
    DETECT_OTHER_APP_ACTIVE = 4
} detector_state_t;

typedef struct {
    detector_state_t state;
    char title_id[16];
    uint32_t game_pid;
    uint32_t game_app_id;
    uint32_t video_pid;
    uint32_t video_app_id;
    uint32_t controller_focus;
    char focus_scene[96];
    char evidence[160];
    int has_game_process;
    int log_truncated;
} detector_result_t;

/* Decode the top-level JSON string field `content` from ShadowMount's
 * /api/v1/kernel-log response. Returns 0 on success. */
int detector_json_content(const char *json, char *out, size_t out_size);
int detector_find_game_name(const char *games_json, const char *title_id,
                            char *out, size_t out_size);

/* Parse the chronological kernel-log text returned by ShadowMountPlus.
 * The function is conservative: it reports UNKNOWN when evidence conflicts. */
int detector_parse_kernel_log(const char *log_text, int truncated,
                              detector_result_t *out);

const char *detector_state_name(detector_state_t state);

#ifdef __cplusplus
}
#endif
#endif
