#include "../rg_gui.h"

/**
 * This file can be edited to add fonts to retro-go.
 * To create new fonts you can use font_converter.py located in the tools folder.
 *
 * CJK fonts: the upstream font_converter.py (tkinter) can't emit the compact
 * rg_font_t binary format, so tools/gen_cjk_font.py does that instead:
 *
 *     python tools/gen_cjk_font.py --font <your-font.ttf> --size 12 \
 *            --symbol font_pixel12 --label "Pixel 12" --out components/retro-go/fonts/pixel12.c
 *
 * then add one extern, one enum value and one table entry below. Append at the
 * END of the enum (before RG_FONT_MAX): the numeric value is what the settings
 * file stores, so inserting in the middle silently changes everyone's font.
 *
 * fonts/pixel12.c ships here as an example. It is a 12px bitmap rendering of
 * Fusion Pixel Font (OFL-1.1, (c) 2022 TakWolf) covering ASCII + GB2312 level-1
 * hanzi; see the header of that file for the full license notice.
 */
extern const rg_font_t font_basic8x8;
extern const rg_font_t font_DejaVu12;
extern const rg_font_t font_DejaVu15;
extern const rg_font_t font_VeraBold11;
extern const rg_font_t font_VeraBold14;
extern const rg_font_t font_pixel12;

enum {
    RG_FONT_BASIC_8,
    RG_FONT_BASIC_12,
    RG_FONT_BASIC_16,
    RG_FONT_DEJAVU_12,
    RG_FONT_DEJAVU_15,
    RG_FONT_VERA_11,
    RG_FONT_VERA_14,
    RG_FONT_PIXEL_12,
    RG_FONT_MAX,
};

static const rg_font_t *fonts[RG_FONT_MAX] = {
    &font_basic8x8,
    &font_basic8x8,
    &font_basic8x8,
    &font_DejaVu12,
    &font_DejaVu15,
    &font_VeraBold11,
    &font_VeraBold14,
    &font_pixel12,
};
