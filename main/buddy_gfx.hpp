#pragma once

#include <stdint.h>
#include <stddef.h>

class GFXcanvas16 {
public:
    bool begin(int16_t w, int16_t h);
    void end();
    int16_t width() const { return w_; }
    int16_t height() const { return h_; }
    uint16_t *getBuffer() { return buf_; }

    void fillScreen(uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t r, uint16_t color);

    void setTextColor(uint16_t color) { text_color_ = color; }
    void setTextSize(uint8_t size) { text_size_ = size ? size : 1; }
    void setCursor(int16_t x, int16_t y) { cx_ = x; cy_ = y; }
    void print(const char *s);
    void print_fit(const char *s, int16_t max_w, uint32_t ms);
    int16_t text_width(const char *s) const;

private:
    void write_char(uint8_t c);
    void write_cjk(uint32_t cp, uint32_t next);
    int16_t glyph_advance(uint32_t cp) const;
    void fill_circle_helper(int16_t x, int16_t y, int16_t r,
                            uint8_t corners, int16_t delta, uint16_t color);

    uint16_t *buf_ = nullptr;
    int16_t w_ = 0;
    int16_t h_ = 0;
    int16_t cx_ = 0;
    int16_t cy_ = 0;
    int16_t clip_l_ = -32768;
    int16_t clip_r_ = 32767;
    uint16_t text_color_ = 0xFFFF;
    uint8_t text_size_ = 1;
};
