#include "buddy_protocol.h"

#include <stdio.h>
#include <string.h>

static int s_fail;

static void expect(int ok, const char *msg)
{
    if (!ok) {
        printf("FAIL %s\n", msg);
        s_fail++;
    }
}

int main(void)
{
    buddy_frame_t f;
    const uint8_t pair[] = { 0xE0, 1, 2, 3, 4, 5, 6 };
    expect(buddy_parse_frame(pair, sizeof(pair), &f) && f.kind == BUDDY_KIND_PAIR_REQ
           && f.host_id[5] == 6, "pair");

    const uint8_t unpair[] = { 0xE1, 1, 2, 3, 4, 5, 6 };
    expect(buddy_parse_frame(unpair, sizeof(unpair), &f) && f.kind == BUDDY_KIND_UNPAIR, "unpair");

    const uint8_t br[] = { 0xFE, 80 };
    expect(buddy_parse_frame(br, sizeof(br), &f) && f.kind == BUDDY_KIND_BRIGHTNESS
           && f.brightness == 80, "bright");

    const uint8_t lo[] = { 0xFE, 1 };
    expect(buddy_parse_frame(lo, sizeof(lo), &f) && f.brightness == 10, "bright min");

    const uint8_t ori[] = { 0xFD, 1 };
    expect(buddy_parse_frame(ori, sizeof(ori), &f) && f.kind == BUDDY_KIND_ORIENTATION
           && f.orientation == 1, "orient");

    const uint8_t ws[] = { 0xFC, 3, 'a', 'p', 'p' };
    expect(buddy_parse_frame(ws, sizeof(ws), &f) && f.kind == BUDDY_KIND_WORKSPACE
           && strcmp(f.workspace, "app") == 0, "ws");

    const uint8_t md[] = { 0xF9, 3, 'g', 'p', 't' };
    expect(buddy_parse_frame(md, sizeof(md), &f) && f.kind == BUDDY_KIND_MODEL
           && strcmp(f.model, "gpt") == 0, "model");

    uint8_t longws[2 + 29];
    longws[0] = 0xFC;
    longws[1] = 29;
    memcpy(longws + 2, "ai-passport-code-island-buddy", 29);
    expect(buddy_parse_frame(longws, sizeof(longws), &f) && f.kind == BUDDY_KIND_WORKSPACE
           && strcmp(f.workspace, "ai-passport-code-island-buddy") == 0, "ws long");

    const uint8_t st[] = { 0xFA, 2, 5, 0x01, 0x02, 9 };
    expect(buddy_parse_frame(st, sizeof(st), &f) && f.kind == BUDDY_KIND_STATS
           && f.tool_calls == 0x0102 && f.duration_min == 9, "stats");

    const uint8_t sa[] = { 0xF8, 3 };
    expect(buddy_parse_frame(sa, sizeof(sa), &f) && f.kind == BUDDY_KIND_SUBAGENT
           && f.subagent_count == 3, "sub");

    const uint8_t ev[] = { 0xF7, 1 };
    expect(buddy_parse_frame(ev, sizeof(ev), &f) && f.kind == BUDDY_KIND_EVENT
           && f.event_id == 1, "event");

    const uint8_t hr[] = { 0xF6, 22 };
    expect(buddy_parse_frame(hr, sizeof(hr), &f) && f.kind == BUDDY_KIND_TIME_HINT
           && f.hour == 22, "hour");

    const uint8_t clr[] = { 0xF5, 0, 0 };
    expect(buddy_parse_frame(clr, sizeof(clr), &f) && f.kind == BUDDY_KIND_TOOL_HIST_CLEAR, "hist clr");

    const uint8_t hist[] = { 0xF5, 0, 0x84, 'B', 'a', 's', 'h' };
    expect(buddy_parse_frame(hist, sizeof(hist), &f) && f.kind == BUDDY_KIND_TOOL_HIST
           && f.hist_success && strcmp(f.hist_name, "Bash") == 0, "hist");

    const uint8_t ag[] = { 3, 2, 4, 'E', 'd', 'i', 't' };
    expect(buddy_parse_frame(ag, sizeof(ag), &f) && f.kind == BUDDY_KIND_AGENT
           && f.source_id == 3 && f.status_id == 2 && strcmp(f.tool, "Edit") == 0, "agent");

    expect(buddy_status_to_scene(0) == BUDDY_SCENE_SLEEP, "sleep");
    expect(buddy_status_to_scene(1) == BUDDY_SCENE_WORK, "work");
    expect(buddy_status_to_scene(3) == BUDDY_SCENE_ALERT, "alert");
    expect(buddy_status_to_scene(4) == BUDDY_SCENE_QUESTION, "q");
    expect(!buddy_parse_frame(ag, 1, &f), "short");

    if (s_fail) {
        printf("%d failed\n", s_fail);
        return 1;
    }
    printf("ok\n");
    return 0;
}
