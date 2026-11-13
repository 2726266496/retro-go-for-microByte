// microByte boot animation.
//
// A port of components/boot_screen/ from the stock (ESP-IDF 4.1) firmware.
// The original is entirely procedural: no frames are stored anywhere, each
// frame is generated on the fly from a 1024-entry sine table plus a dither
// table that is re-randomized every frame. Only 4596 bytes of that stock
// component are data (sin_table.h) - the other 12MB is a vendored FreeType
// tree that existed solely to render the two words "micro" and "BYTE" at an
// arbitrary size.
//
// So this port does two things differently:
//
//   1. The wordmark is baked offline into a 2-bit grey mask
//      (tools/gen_splash_logo.py -> bitmaps/image_splash_text.h). Same
//      Ubuntu-R at the same 30px, same baseline maths as the stock
//      font_render.c, and the same four coverage levels the stock
//      draw_gray2_bitmap() blends with, so it looks identical - for 836
//      bytes of flash instead of a font engine.
//
//   2. Frames are pushed through the normal rg_display_write_rect() path,
//      20 rows at a time, exactly like the stock ST7789_BUFFER_SIZE of 20.
//      rg_display_write_rect() is blocking (it ends in lcd_sync()), so the
//      SPI transfer paces the loop on its own - the stock loop has no
//      delay of its own either.
//
// Everything else is a line-for-line port: the plasma field, the per-frame
// dither, the fade-in over the first 64 frames, and the 100-frame length.
//
// Timing: the stock firmware runs this for a flat 2000ms and then deletes
// the task, so it gets cut off wherever it happens to be. Here all 100
// frames are guaranteed to play, paced to at most SPLASH_FRAME_MS each: the
// splash therefore lasts max(1.5s, however long the SPI actually takes). On
// this device 40MHz SPI makes a frame take roughly 10-25ms, so expect
// ~1.5-2.5s.

#include <stdlib.h>

#include "rg_system.h"
#include "rg_gui.h"
#include "rg_display.h"
#include "rg_splash.h"

#include "bitmaps/image_splash_text.h"
#include "splash_sin_table.h"

#define SPLASH_FRAMES       100
#define SPLASH_FRAME_MS     15 // 100 * 15ms = 1.5s
#define SPLASH_BAND_HEIGHT  20 // rows per SPI transfer (stock value)

// 240 * 20 * 2 = 9600 bytes of .bss, versus 115KB for a full-screen buffer.
static uint16_t splash_band[RG_SCREEN_WIDTH * SPLASH_BAND_HEIGHT];
static uint8_t splash_dither[256];

static inline uint8_t splash_sin(int value)
{
    return sin_table[value & 0x3ff];
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// The stock randomizes the whole 256-byte table once per frame, so the
// dither pattern is different for every frame (which is what keeps the
// plasma from sitting still). rand() here is the same unseeded libc rand()
// the stock used.
static void randomize_dither_table(void)
{
    for (size_t i = 0; i < sizeof(splash_dither); ++i)
        splash_dither[i] = rand() & 0xff;
}

static inline uint16_t dither_color(uint8_t r, uint8_t g, uint8_t b, uint16_t x, uint16_t y)
{
    const uint8_t pos = ((y << 8) + (y << 3) + x) & 0xff;
    uint8_t rand_b = splash_dither[pos];
    const uint8_t rand_r = rand_b & 0x07;
    rand_b >>= 3;
    const uint8_t rand_g = rand_b & 0x03;
    rand_b >>= 2;

    if (r < 249) r = r + rand_r;
    if (g < 253) g = g + rand_g;
    if (b < 249) b = b + rand_b;

    return rgb565(r, g, b);
}

// Two overlapping sine fields, one feeding R and one feeding B, with G
// derived from both. `frame` walks the field, and for the first 64 frames
// the colour is scaled down so the plasma fades up out of black.
static void plasma_band(int band_y, int frame)
{
    const int plasma_shift = frame < 256 ? 1 : 2;
    const int frame_1 = frame << 1, frame_2 = frame << 2, frame_7 = frame * 7;
    uint16_t *out = splash_band;

    for (int row = 0; row < SPLASH_BAND_HEIGHT; ++row)
    {
        const int cy = band_y + row;
        const int cy_1 = cy << 1, cy_2 = cy << 2;

        for (int cx = 0; cx < RG_SCREEN_WIDTH; ++cx)
        {
            const int cx_1 = cx << 1, cx_2 = cx << 2;

            uint16_t plasma_value = splash_sin(cx_2 + cy_1 + frame_2);
            plasma_value += splash_sin(splash_sin(((cy_1 + frame) << 1) + cx) + frame_7);
            plasma_value >>= plasma_shift;
            uint16_t color_r = plasma_value;

            plasma_value = splash_sin(cx + cy_2 + frame_1);
            plasma_value += splash_sin(splash_sin(((cx_1 + frame) << 1) + cy) + frame_1);
            plasma_value >>= plasma_shift;
            uint16_t color_b = plasma_value;

            if (frame < 64)
            {
                color_r = (color_r * frame) >> 6;
                color_b = (color_b * frame) >> 6;
            }
            // (The stock's other fade branch fires only when frame > 128,
            //  which never happens: the animation is 100 frames long.)

            *out++ = dither_color(color_r, (color_r >> 1) + (color_b >> 1), color_b, cx, cy);
        }
    }
}

// Blend white over a native RGB565 pixel with the same weights the stock
// draw_gray2_bitmap() uses in 8-bit space: 50%, 75%, 100%. Done straight in
// the panel's 5/6/5 bit channels, which is arithmetically the same thing.
static inline uint16_t blend_white(uint16_t color, uint8_t level)
{
    uint16_t r = (color >> 11) & 0x1f;
    uint16_t g = (color >> 5) & 0x3f;
    uint16_t b = color & 0x1f;

    if (level == 1)
    {
        r = (r + 31) >> 1;
        g = (g + 63) >> 1;
        b = (b + 31) >> 1;
    }
    else if (level == 2)
    {
        r = (r + 31 * 3) >> 2;
        g = (g + 63 * 3) >> 2;
        b = (b + 31 * 3) >> 2;
    }
    else
    {
        r = 31; g = 63; b = 31;
    }

    return (r << 11) | (g << 5) | b;
}

// Text is drawn on top of the plasma for every frame, un-faded - the stock
// does the same (plasma fades in, the wordmark is already there).
static void text_overlay(int band_y)
{
    const uint8_t *src = splash_text_mask;

    for (int row = 0; row < SPLASH_TEXT_H; ++row)
    {
        const int screen_y = SPLASH_TEXT_Y + row;
        if (screen_y < band_y || screen_y >= band_y + SPLASH_BAND_HEIGHT)
            continue;

        uint16_t *dst = splash_band + (screen_y - band_y) * RG_SCREEN_WIDTH + SPLASH_TEXT_X;
        const uint8_t *src_row = src + row * SPLASH_TEXT_STRIDE;

        for (int col = 0; col < SPLASH_TEXT_W; ++col)
        {
            // 2 bits per pixel, most significant pixel first.
            uint8_t level = (src_row[col >> 2] >> (6 - ((col & 3) << 1))) & 0x03;
            if (level)
                dst[col] = blend_white(dst[col], level);
        }
    }
}

void rg_splash_play(void)
{
    const int64_t time_start = rg_system_timer();

    for (int frame = 0; frame < SPLASH_FRAMES; ++frame)
    {
        randomize_dither_table();

        for (int y = 0; y < RG_SCREEN_HEIGHT; y += SPLASH_BAND_HEIGHT)
        {
            plasma_band(y, frame);
            text_overlay(y);
            rg_display_write_rect(0, y, RG_SCREEN_WIDTH, SPLASH_BAND_HEIGHT,
                                  RG_SCREEN_WIDTH * (int)sizeof(uint16_t), splash_band, 0);
        }

        // Hold this frame until its slot is up. If the SPI was slower than
        // the target, we just move on - no frames are dropped, the whole
        // thing simply takes longer than 1.5s.
        const int64_t deadline = time_start + (int64_t)(frame + 1) * SPLASH_FRAME_MS * 1000;
        const int64_t now = rg_system_timer();
        if (now < deadline)
            rg_task_delay((deadline - now) / 1000);
    }

    // rg_gui_init() doesn't touch the panel and rg_gui_draw_hourglass()
    // paints over whatever is there, so without this the hourglass would
    // land on top of a frozen plasma frame.
    rg_display_clear(C_BLACK);
}
