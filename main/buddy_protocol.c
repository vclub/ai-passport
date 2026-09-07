#include "buddy_protocol.h"

#include <string.h>

uint8_t buddy_clamp_brightness(uint8_t percent)
{
    if (percent < BUDDY_BRIGHTNESS_MIN) return BUDDY_BRIGHTNESS_MIN;
    if (percent > BUDDY_BRIGHTNESS_MAX) return BUDDY_BRIGHTNESS_MAX;
    return percent;
}

uint8_t buddy_clamp_orientation(uint8_t orientation)
{
    return orientation == 1 ? 1 : 0;
}

bool buddy_host_id_eq(const uint8_t *a, const uint8_t *b)
{
    return a && b && memcmp(a, b, BUDDY_HOST_ID_LEN) == 0;
}

buddy_scene_t buddy_status_to_scene(uint8_t status)
{
    switch (status) {
    case 1:
    case 2:
        return BUDDY_SCENE_WORK;
    case 3:
        return BUDDY_SCENE_ALERT;
    case 4:
        return BUDDY_SCENE_QUESTION;
    default:
        return BUDDY_SCENE_SLEEP;
    }
}

static size_t utf8_valid_len(const char *s, size_t n)
{
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t need = 1;
        if (c < 0x80) need = 1;
        else if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else break;
        if (i + need > n) break;
        i += need;
    }
    return i;
}

static void copy_text(char *dst, size_t dst_n, const uint8_t *src, size_t n)
{
    if (n >= dst_n) n = dst_n - 1;
    if (n) memcpy(dst, src, n);
    dst[utf8_valid_len(dst, n)] = '\0';
}

bool buddy_parse_frame(const uint8_t *data, size_t len, buddy_frame_t *out)
{
    if (!data || !out || len < 1) return false;
    memset(out, 0, sizeof(*out));

    if (len >= 1 + BUDDY_HOST_ID_LEN && data[0] == BUDDY_PAIR_REQ) {
        out->kind = BUDDY_KIND_PAIR_REQ;
        memcpy(out->host_id, data + 1, BUDDY_HOST_ID_LEN);
        return true;
    }
    if (len >= 1 + BUDDY_HOST_ID_LEN && data[0] == BUDDY_UNPAIR) {
        out->kind = BUDDY_KIND_UNPAIR;
        memcpy(out->host_id, data + 1, BUDDY_HOST_ID_LEN);
        return true;
    }
    if (len == 2 && data[0] == BUDDY_BRIGHTNESS) {
        out->kind = BUDDY_KIND_BRIGHTNESS;
        out->brightness = buddy_clamp_brightness(data[1]);
        return true;
    }
    if (len == 2 && data[0] == BUDDY_ORIENTATION) {
        out->kind = BUDDY_KIND_ORIENTATION;
        out->orientation = buddy_clamp_orientation(data[1]);
        return true;
    }
    if (len >= 2 && data[0] == BUDDY_WORKSPACE) {
        uint8_t n = data[1];
        if (n > BUDDY_WORKSPACE_MAX) n = BUDDY_WORKSPACE_MAX;
        if (len < 2u + n) return false;
        out->kind = BUDDY_KIND_WORKSPACE;
        out->workspace_len = n;
        copy_text(out->workspace, sizeof(out->workspace), data + 2, n);
        return true;
    }
    if (len >= 2 && data[0] == BUDDY_MODEL) {
        uint8_t n = data[1];
        if (n > BUDDY_MODEL_NAME_MAX) n = BUDDY_MODEL_NAME_MAX;
        if (len < 2u + n) return false;
        out->kind = BUDDY_KIND_MODEL;
        out->model_len = n;
        copy_text(out->model, sizeof(out->model), data + 2, n);
        return true;
    }
    if (len >= 6 && data[0] == BUDDY_STATS) {
        out->kind = BUDDY_KIND_STATS;
        out->active_sessions = data[1];
        out->total_sessions = data[2];
        out->tool_calls = ((uint16_t)data[3] << 8) | data[4];
        out->duration_min = data[5];
        return true;
    }
    if (len >= 2 && data[0] == BUDDY_SUBAGENT) {
        out->kind = BUDDY_KIND_SUBAGENT;
        out->subagent_count = data[1];
        return true;
    }
    if (len >= 2 && data[0] == BUDDY_EVENT) {
        out->kind = BUDDY_KIND_EVENT;
        out->event_id = data[1];
        return true;
    }
    if (len >= 2 && data[0] == BUDDY_TIME_HINT) {
        out->kind = BUDDY_KIND_TIME_HINT;
        out->hour = data[1] > 23 ? 23 : data[1];
        return true;
    }
    if (len >= 4 && data[0] == BUDDY_MSG_PREVIEW) {
        out->kind = BUDDY_KIND_MSG_PREVIEW;
        out->msg_index = data[1];
        out->msg_total = data[2];
        return true;
    }
    if (len >= 3 && data[0] == BUDDY_TOOL_HIST) {
        uint8_t n = data[2] & 0x7F;
        if (n > BUDDY_TOOL_HIST_NAME_MAX) n = BUDDY_TOOL_HIST_NAME_MAX;
        if (data[1] == 0 && n == 0) {
            out->kind = BUDDY_KIND_TOOL_HIST_CLEAR;
            return true;
        }
        if (len < 3u + n) return false;
        out->kind = BUDDY_KIND_TOOL_HIST;
        out->hist_index = data[1];
        out->hist_success = (data[2] & 0x80) != 0;
        out->hist_name_len = n;
        copy_text(out->hist_name, sizeof(out->hist_name), data + 3, n);
        return true;
    }
    if (len >= 2 && data[0] < 0xE0 && (len == 2 || data[2] <= BUDDY_TOOL_NAME_MAX)) {
        uint8_t n = (len >= 3) ? data[2] : 0;
        if (n > BUDDY_TOOL_NAME_MAX) n = BUDDY_TOOL_NAME_MAX;
        if (len == 2 || len >= 3u + n) {
            out->kind = BUDDY_KIND_AGENT;
            out->source_id = data[0];
            out->status_id = data[1];
            out->tool_len = n;
            if (len >= 3) copy_text(out->tool, sizeof(out->tool), data + 3, n);
            return true;
        }
    }
    if (len < 3) return false;
    if (data[0] >= 0xE0) return false;

    uint8_t n = data[2];
    if (n > BUDDY_TOOL_NAME_MAX) n = BUDDY_TOOL_NAME_MAX;
    if (len < 3u + n) return false;
    out->kind = BUDDY_KIND_AGENT;
    out->source_id = data[0];
    out->status_id = data[1];
    out->tool_len = n;
    copy_text(out->tool, sizeof(out->tool), data + 3, n);
    return true;
}
