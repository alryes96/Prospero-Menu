/* PS5 Game Detector: ProsperoMenu overlay. Read-only; no kernel offsets. */
#include "pmo.h"
#include "detector.h"
#include "http_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define JSON_CAP (600u * 1024u)
#define LOG_CAP  (270u * 1024u)

static char api_json[JSON_CAP];
static char log_text[LOG_CAP];
static char games_json[JSON_CAP];
static char game_name[96] = "Name not loaded";
static char name_for_title[16] = "";
static detector_result_t current;
static char connection_text[64] = "Not checked";
static char truncated_text[24] = "Unknown";
static time_t last_success = 0;
static int last_error = 0;

static void refresh_detector(void) {
    if (shadowmount_get_kernel_log(api_json, sizeof api_json) != 0) {
        last_error = 1;
        snprintf(connection_text, sizeof connection_text, "ShadowMount API offline");
        current.state = DETECT_UNKNOWN;
        snprintf(current.evidence, sizeof current.evidence, "Could not read 127.0.0.1:10101/kernel-log");
        return;
    }
    if (detector_json_content(api_json, log_text, sizeof log_text) != 0) {
        last_error = 2;
        snprintf(connection_text, sizeof connection_text, "Invalid API response");
        current.state = DETECT_UNKNOWN;
        snprintf(current.evidence, sizeof current.evidence, "Kernel-log JSON did not contain a readable content field");
        return;
    }
    int truncated = strstr(api_json, "\"truncated\":true") != NULL || strstr(api_json, "\"truncated\": true") != NULL;
    (void)detector_parse_kernel_log(log_text, truncated, &current);
    if (current.title_id[0] && strcmp(name_for_title, current.title_id) != 0) {
        if (shadowmount_get_games(games_json, sizeof games_json) == 0 &&
            detector_find_game_name(games_json, current.title_id, game_name, sizeof game_name) == 0) {
            snprintf(name_for_title, sizeof name_for_title, "%s", current.title_id);
        } else {
            snprintf(game_name, sizeof game_name, "Name unavailable");
        }
    }
    if (!current.title_id[0]) { game_name[0] = '\0'; name_for_title[0] = '\0'; }
    last_error = 0; last_success = time(NULL);
    snprintf(connection_text, sizeof connection_text, "ShadowMount API connected");
    snprintf(truncated_text, sizeof truncated_text, "%s", truncated ? "Yes" : "No");
}

static void render_page(pmo_t *o) {
    char pid[24], app[24], focus[24], video[48], age[32];
    snprintf(pid, sizeof pid, "%u", current.game_pid);
    snprintf(app, sizeof app, "0x%08x", current.game_app_id);
    if (current.controller_focus) snprintf(focus, sizeof focus, "0x%08x", current.controller_focus);
    else snprintf(focus, sizeof focus, "Not available");
    snprintf(video, sizeof video, "PID %u / AppID 0x%08x", current.video_pid, current.video_app_id);
    if (last_success) snprintf(age, sizeof age, "%ld sec ago", (long)(time(NULL) - last_success));
    else snprintf(age, sizeof age, "Never");

    pmo_page_begin(o, "PS5 Game Detector", "Read-only diagnostic / v0.1");
    pmo_header(o, "Detected game");
    pmo_item(o, "state", "Game state", detector_state_name(current.state), current.evidence);
    pmo_item(o, "name", "Game name", game_name[0] ? game_name : "Not detected", "Name is matched against the ShadowMountPlus game catalog.");
    pmo_item(o, "title", "Title ID", current.title_id[0] ? current.title_id : "Not detected", "Detected from recent PS5 lifecycle events.");
    pmo_item(o, "pid", "Game PID", current.game_pid ? pid : "--", "Process ID reported by OnionHEN lifecycle event.");
    pmo_item(o, "appid", "Game AppID", current.game_app_id ? app : "--", "App ID reported by the lifecycle event.");
    pmo_header(o, "Foreground evidence");
    pmo_item(o, "focus", "Controller focus", current.controller_focus ? focus : "Not available", "Latest SetControllerFocus event in the log tail.");
    pmo_item(o, "video", "Video output", current.video_pid ? video : "Not available", "Latest VideoOut shared event in the log tail.");
    pmo_item(o, "scene", "ShellUI scene", current.focus_scene[0] ? current.focus_scene : "Not available", "Latest OnFocusActiveSceneChanged destination.");
    pmo_header(o, "API diagnostics");
    pmo_item(o, "api", "ShadowMount API", connection_text, "Reads 127.0.0.1:10101; no LAN request is made.");
    pmo_item(o, "trunc", "Log truncated", truncated_text, "A truncated log can reduce detection confidence.");
    pmo_item(o, "updated", "Last update", age, NULL);
    pmo_item(o, "refresh", "Refresh now", NULL, "Select to query ShadowMountPlus again.");
    pmo_footer(o, "X Refresh   Circle Back");
    pmo_page_end(o);
}

static void patch_live_values(pmo_t *o) {
    char pid[24], app[24], focus[24], video[48], scene[96];
    snprintf(pid, sizeof pid, "%u", current.game_pid);
    snprintf(app, sizeof app, "0x%08x", current.game_app_id);
    if (current.controller_focus) snprintf(focus, sizeof focus, "0x%08x", current.controller_focus);
    else snprintf(focus, sizeof focus, "Not available");
    if (current.video_pid) snprintf(video, sizeof video, "PID %u / AppID 0x%08x", current.video_pid, current.video_app_id);
    else snprintf(video, sizeof video, "Not available");
    snprintf(scene, sizeof scene, "%s", current.focus_scene[0] ? current.focus_scene : "Not available");
    pmo_patch_value(o, "state", detector_state_name(current.state));
    pmo_patch_detail(o, "state", current.evidence);
    pmo_patch_value(o, "name", game_name[0] ? game_name : "Not detected");
    pmo_patch_value(o, "title", current.title_id[0] ? current.title_id : "Not detected");
    pmo_patch_value(o, "pid", current.game_pid ? pid : "--");
    pmo_patch_value(o, "appid", current.game_app_id ? app : "--");
    pmo_patch_value(o, "focus", focus);
    pmo_patch_value(o, "video", video);
    pmo_patch_value(o, "scene", scene);
    pmo_patch_value(o, "api", connection_text);
    pmo_patch_value(o, "trunc", truncated_text);
    pmo_patch_value(o, "updated", last_success ? "Just refreshed" : "Never");
}

int main(void) {
    pmo_t *o = pmo_connect(NULL);
    if (!o) return 1;
    refresh_detector();
    render_page(o);
    time_t last_poll = time(NULL);
    for (;;) {
        pmo_event_t ev;
        int r = pmo_wait(o, &ev, 500);
        if (r < 0) break;
        if (r > 0) {
            if (ev.kind == PMO_EV_QUIT) break;
            if (ev.kind == PMO_EV_BACK) { pmo_exit(o); continue; }
            if (ev.kind == PMO_EV_SHOW) { refresh_detector(); render_page(o); last_poll = time(NULL); continue; }
            if (ev.kind == PMO_EV_SELECT && strcmp(ev.id, "refresh") == 0) {
                refresh_detector(); render_page(o); last_poll = time(NULL); continue;
            }
        }
        if (pmo_visible(o) && time(NULL) - last_poll >= 2) {
            refresh_detector();
            patch_live_values(o);
            last_poll = time(NULL);
        }
        (void)last_error;
    }
    pmo_close(o);
    return 0;
}
