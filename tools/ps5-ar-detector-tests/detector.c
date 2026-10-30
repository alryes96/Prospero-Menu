#include "detector.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (!src) return;
    snprintf(dst, cap, "%s", src);
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int append_utf8(char *out, size_t cap, size_t *used, unsigned long cp) {
    unsigned char b[4]; size_t n;
    if (cp <= 0x7f) { b[0] = (unsigned char)cp; n = 1; }
    else if (cp <= 0x7ff) { b[0] = 0xc0 | (unsigned char)(cp >> 6); b[1] = 0x80 | (unsigned char)(cp & 0x3f); n = 2; }
    else if (cp <= 0xffff) { b[0] = 0xe0 | (unsigned char)(cp >> 12); b[1] = 0x80 | (unsigned char)((cp >> 6) & 0x3f); b[2] = 0x80 | (unsigned char)(cp & 0x3f); n = 3; }
    else if (cp <= 0x10ffff) { b[0] = 0xf0 | (unsigned char)(cp >> 18); b[1] = 0x80 | (unsigned char)((cp >> 12) & 0x3f); b[2] = 0x80 | (unsigned char)((cp >> 6) & 0x3f); b[3] = 0x80 | (unsigned char)(cp & 0x3f); n = 4; }
    else return -1;
    if (*used + n >= cap) return -1;
    memcpy(out + *used, b, n); *used += n; out[*used] = '\0';
    return 0;
}

static int read_json_string(const char **cursor, char *out, size_t cap) {
    const char *p = *cursor;
    size_t used = 0;
    if (*p != '"' || cap == 0) return -1;
    p++; out[0] = '\0';
    while (*p && *p != '"') {
        unsigned long cp;
        if ((unsigned char)*p < 0x20) return -1;
        if (*p != '\\') {
            if (used + 1 >= cap) return -1;
            out[used++] = *p++; out[used] = '\0';
            continue;
        }
        p++;
        if (!*p) return -1;
        switch (*p) {
            case '"': case '\\': case '/':
                if (used + 1 >= cap) return -1;
                out[used++] = *p++; out[used] = '\0'; break;
            case 'b': if (used + 1 >= cap) return -1; out[used++] = '\b'; p++; out[used] = '\0'; break;
            case 'f': if (used + 1 >= cap) return -1; out[used++] = '\f'; p++; out[used] = '\0'; break;
            case 'n': if (used + 1 >= cap) return -1; out[used++] = '\n'; p++; out[used] = '\0'; break;
            case 'r': if (used + 1 >= cap) return -1; out[used++] = '\r'; p++; out[used] = '\0'; break;
            case 't': if (used + 1 >= cap) return -1; out[used++] = '\t'; p++; out[used] = '\0'; break;
            case 'u': {
                int h[4]; p++;
                for (int i = 0; i < 4; i++) { h[i] = hex_value(p[i]); if (h[i] < 0) return -1; }
                cp = (unsigned long)((h[0] << 12) | (h[1] << 8) | (h[2] << 4) | h[3]); p += 4;
                if (cp >= 0xd800 && cp <= 0xdbff && p[0] == '\\' && p[1] == 'u') {
                    int l[4]; p += 2;
                    for (int i = 0; i < 4; i++) { l[i] = hex_value(p[i]); if (l[i] < 0) return -1; }
                    unsigned long low = (unsigned long)((l[0] << 12) | (l[1] << 8) | (l[2] << 4) | l[3]);
                    if (low < 0xdc00 || low > 0xdfff) return -1;
                    p += 4; cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                }
                if (append_utf8(out, cap, &used, cp)) return -1;
                break;
            }
            default: return -1;
        }
    }
    if (*p != '"') return -1;
    *cursor = p + 1;
    return 0;
}

int detector_json_content(const char *json, char *out, size_t out_size) {
    const char *p;
    if (!json || !out || out_size == 0) return -1;
    out[0] = '\0';
    p = strstr(json, "\"content\"");
    if (!p) return -1;
    p += strlen("\"content\"");
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p++ != ':') return -1;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return -1;
    return read_json_string(&p, out, out_size);
}

int detector_find_game_name(const char *games_json, const char *title_id,
                            char *out, size_t out_size) {
    const char *p = games_json;
    char found_id[32];
    if (!games_json || !title_id || !out || out_size == 0) return -1;
    out[0] = '\0';
    while ((p = strstr(p, "\"title_id\"")) != NULL) {
        const char *q = p + strlen("\"title_id\"");
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q++ != ':') { p = q; continue; }
        while (*q && isspace((unsigned char)*q)) q++;
        if (read_json_string(&q, found_id, sizeof found_id) != 0) { p = q; continue; }
        if (strcmp(found_id, title_id) == 0) {
            const char *object_end = strchr(q, '}');
            const char *name = strstr(q, "\"title_name\"");
            if (!name || (object_end && name > object_end)) return -1;
            name += strlen("\"title_name\"");
            while (*name && isspace((unsigned char)*name)) name++;
            if (*name++ != ':') return -1;
            while (*name && isspace((unsigned char)*name)) name++;
            return read_json_string(&name, out, out_size);
        }
        p = q;
    }
    return -1;
}

static int parse_u32(const char *p, int base, uint32_t *out) {
    char *end = NULL; unsigned long v;
    while (*p && isspace((unsigned char)*p)) p++;
    v = strtoul(p, &end, base);
    if (end == p || v > 0xfffffffful) return -1;
    *out = (uint32_t)v; return 0;
}

static const char *find_after(const char *s, const char *needle) {
    const char *p = strstr(s, needle);
    return p ? p + strlen(needle) : NULL;
}

static int is_title_id_at(const char *s) {
    for (int i = 0; i < 4; i++) if (!s[i] || s[i] < 'A' || s[i] > 'Z') return 0;
    for (int i = 4; i < 9; i++) if (!s[i] || s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

static int find_title_id(const char *s, char out[16]) {
    for (const char *p = s; *p; p++) {
        if (is_title_id_at(p)) { memcpy(out, p, 9); out[9] = '\0'; return 0; }
    }
    return -1;
}

static void parse_video(const char *line, detector_result_t *r) {
    const char *p = find_after(line, "VideoOut: shared (pid=0x");
    uint32_t pid, app;
    if (!p || parse_u32(p, 16, &pid)) return;
    p = find_after(p, "appId=0x");
    if (!p || parse_u32(p, 16, &app)) return;
    r->video_pid = pid; r->video_app_id = app;
}

static void parse_focus(const char *line, detector_result_t *r, int *focus_valid) {
    const char *p = find_after(line, "SetControllerFocus(");
    uint32_t value;
    if (!p) return;
    /* -1 is emitted during focus transitions and invalidates the previous
     * focus value. Keeping an older AppID here can falsely mark a background
     * game as foreground when the log tail ends during a transition. */
    if (*p == '-') {
        r->controller_focus = 0;
        *focus_valid = 0;
        return;
    }
    if (parse_u32(p, 0, &value)) return;
    r->controller_focus = value; *focus_valid = 1;
}

static void parse_onion_start(const char *line, detector_result_t *r) {
    const char *p = find_after(line, "Big App started pid=");
    uint32_t pid, app;
    const char *a, *t;
    if (!p || parse_u32(p, 10, &pid)) return;
    a = find_after(p, "appid="); t = find_after(p, "tid=");
    if (!a || parse_u32(a, 10, &app) || !t || !is_title_id_at(t)) return;
    memcpy(r->title_id, t, 9); r->title_id[9] = '\0';
    r->game_pid = pid; r->game_app_id = app; r->has_game_process = 1;
    copy_text(r->evidence, sizeof r->evidence, "OnionHEN reported Big App start");
}

static void clear_game_if_matching(const char *line, detector_result_t *r) {
    char title[16];
    const char *p;
    if (strstr(line, "ProcessTerm()") || strstr(line, "Kill App :") || strstr(line, "BlockingKill()")) {
        p = strstr(line, "titleId=");
        if (p) {
            p += 8;
            while (*p == '{' || *p == '[' || isspace((unsigned char)*p)) p++;
            if (is_title_id_at(p)) { memcpy(title, p, 9); title[9] = '\0'; }
            else if (find_title_id(line, title)) title[0] = '\0';
        } else if (find_title_id(line, title)) title[0] = '\0';
        if (title[0] && r->title_id[0] && strcmp(title, r->title_id) == 0) {
            r->has_game_process = 0; r->game_pid = 0; r->game_app_id = 0;
            copy_text(r->evidence, sizeof r->evidence, "Game process termination was logged");
        }
    }
}

int detector_parse_kernel_log(const char *log_text, int truncated, detector_result_t *out) {
    const char *line, *end;
    int focus_valid = 0, scene_seen = 0;
    if (!log_text || !out) return -1;
    memset(out, 0, sizeof *out); out->state = DETECT_UNKNOWN; out->log_truncated = truncated != 0;
    line = log_text; end = log_text + strlen(log_text);
    while (line < end) {
        const char *nl = memchr(line, '\n', (size_t)(end - line));
        size_t n = nl ? (size_t)(nl - line) : (size_t)(end - line);
        char tmp[2048];
        if (n >= sizeof tmp) n = sizeof tmp - 1;
        memcpy(tmp, line, n); tmp[n] = '\0';
        parse_onion_start(tmp, out);
        if (strstr(tmp, "launchApp(")) {
            char title[16]; if (!find_title_id(tmp, title)) {
                copy_text(out->title_id, sizeof out->title_id, title);
                /* launchApp is a launch request, not proof that the process exists. */
            }
        }
        clear_game_if_matching(tmp, out);
        parse_video(tmp, out);
        parse_focus(tmp, out, &focus_valid);
        const char *scene = find_after(tmp, "-> [");
        if (scene) {
            size_t len = strcspn(scene, "]");
            if (len >= sizeof out->focus_scene) len = sizeof out->focus_scene - 1;
            memcpy(out->focus_scene, scene, len); out->focus_scene[len] = '\0'; scene_seen = 1;
        }
        if (nl) line = nl + 1; else break;
    }

    if (!out->has_game_process && out->title_id[0] == '\0') {
        out->state = focus_valid ? DETECT_NO_GAME : DETECT_UNKNOWN;
        copy_text(out->evidence, sizeof out->evidence, focus_valid ? "No game process was found in the log tail" : "No decisive game or focus event in the log tail");
        return 0;
    }
    if (!out->has_game_process && strstr(out->evidence, "termination")) {
        out->state = DETECT_NO_GAME;
        return 0;
    }
    if (!out->has_game_process) {
        out->state = DETECT_UNKNOWN;
        copy_text(out->evidence, sizeof out->evidence, "Title was mentioned, but a live process could not be confirmed");
        return 0;
    }
    if (focus_valid && out->game_app_id && out->controller_focus == out->game_app_id &&
        out->video_app_id == out->game_app_id && out->video_pid == out->game_pid) {
        out->state = DETECT_GAME_ACTIVE;
        copy_text(out->evidence, sizeof out->evidence, "VideoOut PID/AppID and controller focus match the game");
        return 0;
    }
    if (focus_valid && (out->controller_focus == 0x8u || out->controller_focus == 0x7u)) {
        out->state = DETECT_GAME_BACKGROUND;
        copy_text(out->evidence, sizeof out->evidence, "Shell/Home owns controller focus; game process start remains in log");
        return 0;
    }
    if (scene_seen && (strstr(out->focus_scene, "NPXS40002") || strstr(out->focus_scene, "NPXS40003") ||
                       strstr(out->focus_scene, "ReactModalScene") || strstr(out->focus_scene, "ReactPopupMenu"))) {
        out->state = DETECT_GAME_BACKGROUND;
        copy_text(out->evidence, sizeof out->evidence, "Latest ShellUI focus scene is a Shell/modal scene");
        return 0;
    }
    if (focus_valid && out->controller_focus && out->controller_focus != out->game_app_id) {
        out->state = DETECT_OTHER_APP_ACTIVE;
        copy_text(out->evidence, sizeof out->evidence, "Controller focus belongs to a different app");
        return 0;
    }
    out->state = DETECT_UNKNOWN;
    copy_text(out->evidence, sizeof out->evidence, truncated ? "Log is truncated; current foreground state is not decisive" : "Game process is known, but foreground evidence is incomplete");
    return 0;
}

const char *detector_state_name(detector_state_t state) {
    switch (state) {
        case DETECT_NO_GAME: return "No game detected";
        case DETECT_GAME_ACTIVE: return "Game active";
        case DETECT_GAME_BACKGROUND: return "Game in background / Shell";
        case DETECT_OTHER_APP_ACTIVE: return "Another app has focus";
        case DETECT_UNKNOWN: default: return "State uncertain";
    }
}
