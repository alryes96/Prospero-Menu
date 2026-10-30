#include "../detector.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_active(void) {
    const char *log =
        "[SceLncService] launchApp(PPSA23012)\n"
        "[AvControl] VideoOut: shared (pid=0x6e appId=0x4019)\n"
        "[onion_daemon.elf] [OnionHEN] info: [lifecycle] Big App started pid=110 appid=16409 tid=PPSA23012 via=exec source=exec\n"
        "[SceLncService] SetControllerFocus(0x00004019)\n"
        "[SceShellUI] OnFocusActiveSceneChanged [SplashScreen.PPSA23012 : SplashScreen] -> [AppScreen : ApplicationScreenScene]\n";
    detector_result_t r;
    assert(detector_parse_kernel_log(log, 0, &r) == 0);
    assert(r.state == DETECT_GAME_ACTIVE);
    assert(strcmp(r.title_id, "PPSA23012") == 0);
    assert(r.game_pid == 110);
    assert(r.game_app_id == 16409);
    assert(r.controller_focus == 0x4019);
    assert(r.video_pid == 110 && r.video_app_id == 0x4019);
}

static void test_home_with_suspended_game(void) {
    const char *log =
        "[SceLncService] launchApp(PPSA23012)\n"
        "[AvControl] VideoOut: shared (pid=0x6e appId=0x4019)\n"
        "[onion_daemon.elf] [OnionHEN] info: [lifecycle] Big App started pid=110 appid=16409 tid=PPSA23012 via=exec source=exec\n"
        "[SceLncService] SetControllerFocus(0x00004019)\n"
        "[SceShellUI] OnFocusActiveSceneChanged [SplashScreen.PPSA23012 : SplashScreen] -> [AppScreen : ApplicationScreenScene]\n"
        "[SceShellUI] OnFocusActiveSceneChanged [AppScreen : ApplicationScreenScene] -> [Render.NPXS40002 : AppScene]\n"
        "[AvControl] VideoOut: shared (pid=0x3b appId=0x8)\n"
        "[SceLncService] SetControllerFocus(0x00000008)\n";
    detector_result_t r;
    assert(detector_parse_kernel_log(log, 0, &r) == 0);
    assert(r.state == DETECT_GAME_BACKGROUND);
    assert(strcmp(r.title_id, "PPSA23012") == 0);
    assert(r.controller_focus == 0x8);
}

static void test_close_game(void) {
    const char *log =
        "[onion_daemon.elf] [OnionHEN] info: [lifecycle] Big App started pid=110 appid=16409 tid=PPSA23012 via=exec source=exec\n"
        "[sceProcessStarter] ProcessTerm() big/mini app title_id = [PPSA23012] root_dir = [sandbox/PPSA23012_000]\n"
        "[SceLncService] SetControllerFocus(0x00000008)\n";
    detector_result_t r;
    assert(detector_parse_kernel_log(log, 0, &r) == 0);
    assert(r.state == DETECT_NO_GAME);
    assert(!r.has_game_process);
}


static void test_focus_transition_invalidates_previous_focus(void) {
    const char *log =
        "[AvControl] VideoOut: shared (pid=0x6e appId=0x4019)\n"
        "[OnionHEN] info: [lifecycle] Big App started pid=110 appid=16409 tid=PPSA23012\n"
        "[SceLncService] SetControllerFocus(0x00004019)\n"
        "[SceLncService] SetAppFocus(): SetControllerFocus(-1)\n";
    detector_result_t r;
    assert(detector_parse_kernel_log(log, 0, &r) == 0);
    assert(r.state == DETECT_UNKNOWN);
    assert(r.controller_focus == 0);
}

static void test_json_content_escaped(void) {
    const char *json = "{\"status\":0,\"truncated\":false,\"content\":\"line1\\nline2 \\\"quoted\\\"\\\\ tail\"}";
    char out[128];
    assert(detector_json_content(json, out, sizeof out) == 0);
    assert(strcmp(out, "line1\nline2 \"quoted\"\\ tail") == 0);
}

static void test_game_name_lookup(void) {
    const char *json = "{\"status\":0,\"games\":[{\"title_id\":\"PPSA23012\",\"title_name\":\"Call of Duty\\u00ae Modern Warfare 2\"},{\"title_id\":\"CUSA12345\",\"title_name\":\"Other Game\"}]}";
    char name[96];
    assert(detector_find_game_name(json, "PPSA23012", name, sizeof name) == 0);
    assert(strncmp(name, "Call of Duty", 12) == 0);
}

static void test_truncated_marks_low_confidence(void) {
    const char *log = "[OnionHEN] [lifecycle] Big App started pid=110 appid=16409 tid=PPSA23012\n[SceLncService] SetControllerFocus(-1)\n";
    detector_result_t r;
    assert(detector_parse_kernel_log(log, 1, &r) == 0);
    assert(r.state == DETECT_UNKNOWN);
    assert(r.log_truncated == 1);
}

int main(void) {
    test_active();
    test_home_with_suspended_game();
    test_close_game();
    test_focus_transition_invalidates_previous_focus();
    test_json_content_escaped();
    test_game_name_lookup();
    test_truncated_marks_low_confidence();
    puts("All detector tests passed.");
    return 0;
}
