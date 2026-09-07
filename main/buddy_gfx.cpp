#include "buddy_gfx.hpp"

#include "esp_heap_caps.h"
#include "lvgl.h"

#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_cjk_12);

static const char *utf8_next(const char *s, uint32_t *out)
{
    const uint8_t *p = (const uint8_t *)s;
    uint8_t c = p[0];
    if (c < 0x80) {
        *out = c;
        return s + 1;
    }
    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *out = ((uint32_t)(c & 0x1F) << 6) | (p[1] & 0x3F);
        return s + 2;
    }
    if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *out = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return s + 3;
    }
    if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *out = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
               ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return s + 4;
    }
    *out = '?';
    return s + 1;
}

static const uint8_t FONT_5X7[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00,
    0x14,0x7F,0x14,0x7F,0x14, 0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62,
    0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00, 0x00,0x1C,0x22,0x41,0x00,
    0x00,0x41,0x22,0x1C,0x00, 0x08,0x2A,0x1C,0x2A,0x08, 0x08,0x08,0x3E,0x08,0x08,
    0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00,
    0x20,0x10,0x08,0x04,0x02, 0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00,
    0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31, 0x18,0x14,0x12,0x7F,0x10,
    0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
    0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00,
    0x00,0x56,0x36,0x00,0x00, 0x00,0x08,0x14,0x22,0x41, 0x14,0x14,0x14,0x14,0x14,
    0x41,0x22,0x14,0x08,0x00, 0x02,0x01,0x51,0x09,0x06, 0x32,0x49,0x79,0x41,0x3E,
    0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x01,0x01,
    0x3E,0x41,0x41,0x51,0x32, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
    0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
    0x7F,0x02,0x04,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
    0x1F,0x20,0x40,0x20,0x1F, 0x7F,0x20,0x18,0x20,0x7F, 0x63,0x14,0x08,0x14,0x63,
    0x03,0x04,0x78,0x04,0x03, 0x61,0x51,0x49,0x45,0x43, 0x00,0x00,0x7F,0x41,0x41,
    0x02,0x04,0x08,0x10,0x20, 0x41,0x41,0x7F,0x00,0x00, 0x04,0x02,0x01,0x02,0x04,
    0x40,0x40,0x40,0x40,0x40, 0x00,0x01,0x02,0x04,0x00, 0x20,0x54,0x54,0x54,0x78,
    0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20, 0x38,0x44,0x44,0x48,0x7F,
    0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02, 0x08,0x14,0x54,0x54,0x3C,
    0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x44,0x3D,0x00,
    0x00,0x7F,0x10,0x28,0x44, 0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x18,0x04,0x78,
    0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38, 0x7C,0x14,0x14,0x14,0x08,
    0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20,
    0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C,
    0x3C,0x40,0x30,0x40,0x3C, 0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C,
    0x44,0x64,0x54,0x4C,0x44, 0x00,0x08,0x36,0x41,0x00, 0x00,0x00,0x7F,0x00,0x00,
    0x00,0x41,0x36,0x08,0x00, 0x08,0x08,0x2A,0x1C,0x08, 0x08,0x1C,0x2A,0x08,0x08,
};

bool GFXcanvas16::begin(int16_t w, int16_t h)
{
    end();
    if (w <= 0 || h <= 0) return false;
    size_t n = (size_t)w * (size_t)h * 2;
    buf_ = (uint16_t *)heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf_) buf_ = (uint16_t *)malloc(n);
    if (!buf_) return false;
    w_ = w;
    h_ = h;
    memset(buf_, 0, n);
    return true;
}

void GFXcanvas16::end()
{
    free(buf_);
    buf_ = nullptr;
    w_ = 0;
    h_ = 0;
}

void GFXcanvas16::fillScreen(uint16_t color)
{
    if (!buf_) return;
    size_t n = (size_t)w_ * (size_t)h_;
    if (color == 0) {
        memset(buf_, 0, n * 2);
        return;
    }
    for (size_t i = 0; i < n; i++) buf_[i] = color;
}

void GFXcanvas16::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (!buf_ || x < 0 || y < 0 || x >= w_ || y >= h_) return;
    buf_[y * w_ + x] = color;
}

void GFXcanvas16::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    for (;;) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void GFXcanvas16::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (!buf_ || w <= 0 || h <= 0) return;
    int16_t x1 = x + w;
    int16_t y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > w_) x1 = w_;
    if (y1 > h_) y1 = h_;
    for (int16_t yy = y; yy < y1; yy++) {
        uint16_t *row = buf_ + yy * w_ + x;
        for (int16_t xx = x; xx < x1; xx++) *row++ = color;
    }
}

void GFXcanvas16::fill_circle_helper(int16_t x0, int16_t y0, int16_t r,
                                     uint8_t corners, int16_t delta, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddx = 1;
    int16_t ddy = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    int16_t px = x;
    int16_t py = y;

    delta++;
    while (x < y) {
        if (f >= 0) {
            y--;
            ddy += 2;
            f += ddy;
        }
        x++;
        ddx += 2;
        f += ddx;
        if (x < (y + 1)) {
            if (corners & 1) fillRect(x0 + x, y0 - y, 1, 2 * y + delta, color);
            if (corners & 2) fillRect(x0 - x, y0 - y, 1, 2 * y + delta, color);
        }
        if (y != py) {
            if (corners & 1) fillRect(x0 + py, y0 - px, 1, 2 * px + delta, color);
            if (corners & 2) fillRect(x0 - py, y0 - px, 1, 2 * px + delta, color);
            py = y;
        }
        px = x;
    }
}

void GFXcanvas16::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color)
{
    if (r < 0) return;
    fillRect(x, y - r, 1, 2 * r + 1, color);
    fill_circle_helper(x, y, r, 3, 0, color);
}

void GFXcanvas16::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2, uint16_t color)
{
    int16_t a, b, y, last;
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { int16_t t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y0 == y2) {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        fillRect(a, y0, b - a + 1, 1, color);
        return;
    }
    int32_t dx01 = x1 - x0, dy01 = y1 - y0;
    int32_t dx02 = x2 - x0, dy02 = y2 - y0;
    int32_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;
    last = (y1 == y2) ? y1 : y1 - 1;
    for (y = y0; y <= last; y++) {
        a = x0 + sa / (dy01 ? dy01 : 1);
        b = x0 + sb / (dy02 ? dy02 : 1);
        sa += dx01;
        sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        fillRect(a, y, b - a + 1, 1, color);
    }
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = x1 + sa / (dy12 ? dy12 : 1);
        b = x0 + sb / (dy02 ? dy02 : 1);
        sa += dx12;
        sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        fillRect(a, y, b - a + 1, 1, color);
    }
}

void GFXcanvas16::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                int16_t r, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    int16_t max_r = w < h ? w : h;
    max_r /= 2;
    if (r > max_r) r = max_r;
    fillRect(x + r, y, w - 2 * r, h, color);
    if (r <= 0) return;
    fill_circle_helper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    fill_circle_helper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

void GFXcanvas16::write_char(uint8_t c)
{
    if (c < 32 || c > 126) c = '?';
    uint8_t s = text_size_;
    int16_t adv = (int16_t)(6 * s);
    if (cx_ >= clip_l_ && cx_ + adv <= clip_r_) {
        const uint8_t *g = &FONT_5X7[(c - 32) * 5];
        for (int8_t i = 0; i < 5; i++) {
            uint8_t line = g[i];
            for (int8_t j = 0; j < 8; j++, line >>= 1) {
                if (line & 1) fillRect(cx_ + i * s, cy_ + j * s, s, s, text_color_);
            }
        }
    }
    cx_ += adv;
}

void GFXcanvas16::write_cjk(uint32_t cp, uint32_t next)
{
    lv_font_glyph_dsc_t dsc;
    memset(&dsc, 0, sizeof(dsc));
    if (!lv_font_cjk_12.get_glyph_dsc ||
        !lv_font_cjk_12.get_glyph_dsc(&lv_font_cjk_12, &dsc, cp, next) ||
        !dsc.gid.index) {
        write_char('?');
        return;
    }
    uint8_t s = text_size_;
    int16_t adv = (int16_t)dsc.adv_w * s;
    if (cx_ >= clip_l_ && cx_ + adv <= clip_r_ &&
        dsc.box_w && dsc.box_h && lv_font_cjk_12.get_glyph_bitmap) {
        dsc.resolved_font = &lv_font_cjk_12;
        dsc.req_raw_bitmap = 1;
        const uint8_t *bits = (const uint8_t *)lv_font_cjk_12.get_glyph_bitmap(&dsc, nullptr);
        if (bits) {
            int16_t gx = cx_ + (int16_t)dsc.ofs_x * s;
            int16_t gy = cy_;
            uint32_t bit = 0;
            for (uint16_t y = 0; y < dsc.box_h; y++) {
                for (uint16_t x = 0; x < dsc.box_w; x++, bit++) {
                    if (bits[bit >> 3] & (uint8_t)(0x80 >> (bit & 7))) {
                        fillRect(gx + (int16_t)x * s, gy + (int16_t)y * s, s, s, text_color_);
                    }
                }
            }
        }
    }
    cx_ += adv;
}

int16_t GFXcanvas16::glyph_advance(uint32_t cp) const
{
    uint8_t s = text_size_;
    if (cp >= 32 && cp <= 126) return (int16_t)(6 * s);
    lv_font_glyph_dsc_t dsc;
    memset(&dsc, 0, sizeof(dsc));
    if (!lv_font_cjk_12.get_glyph_dsc ||
        !lv_font_cjk_12.get_glyph_dsc(&lv_font_cjk_12, &dsc, cp, 0)) {
        return (int16_t)(6 * s);
    }
    return (int16_t)(dsc.adv_w * s);
}

int16_t GFXcanvas16::text_width(const char *s) const
{
    if (!s) return 0;
    int16_t w = 0;
    while (*s) {
        uint32_t cp;
        s = utf8_next(s, &cp);
        w += glyph_advance(cp);
    }
    return w;
}

void GFXcanvas16::print(const char *s)
{
    if (!s) return;
    while (*s) {
        uint32_t cp = 0;
        uint32_t next = 0;
        s = utf8_next(s, &cp);
        if (*s) utf8_next(s, &next);
        if (cp >= 32 && cp <= 126) write_char((uint8_t)cp);
        else write_cjk(cp, next);
    }
}

void GFXcanvas16::print_fit(const char *s, int16_t max_w, uint32_t ms)
{
    if (!s || max_w <= 0) return;
    int16_t x0 = cx_;
    int16_t y0 = cy_;
    int16_t tw = text_width(s);
    if (tw <= max_w) {
        print(s);
        return;
    }
    int16_t loop = tw + 18 * (int16_t)text_size_;
    int16_t off = (int16_t)((ms / 80) % (uint32_t)loop);
    clip_l_ = x0;
    clip_r_ = (int16_t)(x0 + max_w);
    cx_ = (int16_t)(x0 - off);
    cy_ = y0;
    print(s);
    cx_ = (int16_t)(x0 - off + loop);
    cy_ = y0;
    print(s);
    clip_l_ = -32768;
    clip_r_ = 32767;
    cx_ = x0;
    cy_ = y0;
}
