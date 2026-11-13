#!/usr/bin/env python3
"""
Generate a retro-go rg_font_t C source file containing CJK glyphs.

This is a headless replacement for tools/font_converter.py (which is a tkinter
GUI with a hardcoded 32-126,160-255 charset and cannot emit CJK).

It reproduces the exact binary layout that rg_gui.c:get_glyph() decodes:

    rg_font_glyph_t (packed, 7 byte header)
        uint16 code        LE
        uint8  yOffset
        uint8  width       bitmap width, 0 for blank glyphs (space, etc)
        uint8  height      bitmap height, 0 for blank glyphs
        uint8  xOffset     always >= 0 here
        uint8  xDelta      advance to next glyph
        uint8  data[]      ceil(width*height/8) bytes, tight bitstream,
                           MSB first, no row padding
    list is terminated by a glyph whose code is 0x0000 (2 bytes total).

Character set: ASCII 0x20-0x7E, GB2312 zones 1 and 3 (CJK punctuation and
fullwidth forms) and GB2312 level-1 hanzi (the 3755 most common), which is
what you need for ROM names and UI labels but keeps the font ~160KB.
Code points the source font does not actually contain are dropped and
reported (fontTools is used for the cmap lookup if it is installed).

Usage:
    python tools/gen_cjk_font.py <font_path> [pixel_size] [out.c]
                                 [--symbol NAME] [--label LABEL]
                                 [--copyright TEXT] [--license NAME]

font_path is required - no font is assumed, because the one that suits a
CJK bitmap menu is a licensing decision, not a technical one. Use a font
you are allowed to redistribute if the output ships with your firmware:
the checked-in fonts/pixel12.c was generated from Fusion Pixel Font
(OFL-1.1). Do NOT generate a font from a proprietary system font (e.g.
C:/Windows/Fonts/msyh.ttc, Segoe UI, SF Pro) and publish the .c file: the
generated file embeds rasterized glyph data of that font.

Defaults for the remaining arguments: size 12, output
components/retro-go/fonts/cjk.c, symbol font_cjk, label "CJK 12".

--copyright/--license only add comment lines to the generated header; they
change nothing about the glyph data, so regenerating with or without them
produces a byte-identical .data array.
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

DEFAULT_SIZE = 12
DEFAULT_OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "components", "retro-go", "fonts", "cjk.c",
)

# Glyph symbol used in the C file and in the Font option in the settings menu.
DEFAULT_SYMBOL = "font_cjk"
DEFAULT_LABEL = "CJK 12"


def build_charset():
    """ASCII + GB2312 symbols/fullwidth + GB2312 level-1 hanzi."""
    codes = set(range(0x20, 0x7F))

    def add_gb2312(rows):
        for hi in rows:
            for lo in range(0xA1, 0xFF):
                try:
                    ch = bytes([hi, lo]).decode("gb2312")
                except UnicodeDecodeError:
                    continue  # unassigned code point
                codes.add(ord(ch))

    add_gb2312([0xA1, 0xA3])              # zone 1 punctuation, zone 3 fullwidth
    add_gb2312(range(0xB0, 0xD8))         # level-1 hanzi, 0xB0A1-0xD7FE

    # U+3000 is the ideographic space; make sure whitespace is never a box.
    codes.add(0x20)
    codes.add(0x3000)
    return sorted(codes)


def font_codepoints(font_path, font_number=0):
    """
    Union of every Unicode cmap subtable in the font, i.e. the set of code
    points that actually have a glyph. Returns None if fontTools is missing.

    Matters because a code point with no glyph renders as FreeType's .notdef
    (a hollow box) rather than as retro-go's own "unknown character" marker,
    and it would waste bitmap bytes. Better to drop it and let rg_gui decide.
    """
    try:
        from fontTools.ttLib import TTFont
    except ImportError:
        return None
    tt = TTFont(font_path, fontNumber=font_number, lazy=True)
    covered = set()
    for table in tt["cmap"].tables:
        covered.update(table.cmap.keys())
    return covered


def find_bounding_box(image):
    """Same semantics as font_converter.py: bbox of any non-black pixel."""
    width, height = image.size
    x_min, y_min = width, height
    x_max, y_max = 0, 0
    for y in range(height):
        for x in range(width):
            if image.getpixel((x, y)):
                x_min = min(x_min, x)
                y_min = min(y_min, y)
                x_max = max(x_max, x)
                y_max = max(y_max, y)
    if x_min > x_max or y_min > y_max:
        return None
    return (x_min, y_min, x_max + 1, y_max + 1)


def pack_bitmap(image, width, height):
    """Tight bitstream, MSB first, no per-row padding."""
    bitmap = bytearray()
    row = 0
    nbits = 0
    for y in range(height):
        for x in range(width):
            if nbits == 8:
                bitmap.append(row)
                row = 0
                nbits = 0
            row = (row << 1) | (1 if image.getpixel((x, y)) else 0)
            nbits += 1
    bitmap.append(row << (8 - nbits))
    return bytes(bitmap[: (width * height + 7) // 8])


def render_glyph(pil_font, char_code, font_size):
    char = chr(char_code)
    # Monochrome ("1") + FreeType hinting gives crisp stems for CJK at 16px,
    # much better than thresholding an antialiased render.
    canvas = Image.new("1", (font_size * 2, font_size * 2), 0)
    draw = ImageDraw.Draw(canvas)
    # Draw at x=1 to avoid clipping the left edge of some glyphs.
    draw.text((1, 0), char, font=pil_font, fill=255)

    bbox = find_bounding_box(canvas)
    if bbox is None:  # space / zero-width
        width = height = offset_x = offset_y = 0
    else:
        x0, y0, x1, y1 = bbox
        width, height = x1 - x0, y1 - y0
        offset_x, offset_y = x0, y0
        if offset_x:
            offset_x -= 1
        canvas = canvas.crop(bbox)

    try:
        adv_w = int(draw.textlength(char, font=pil_font))
        adv_w = max(adv_w, width + offset_x)
    except Exception:
        adv_w = width + offset_x

    return {
        "code": char_code,
        "ofs_y": offset_y,
        "w": width,
        "h": height,
        "ofs_x": offset_x,
        "adv": adv_w,
        "bitmap": pack_bitmap(canvas, width, height),
    }


def trim_top_padding(glyphs, font_size):
    """Drop the consistent excess of top padding some fonts have."""
    max_height = max(g["ofs_y"] + g["h"] for g in glyphs)
    if max_height <= font_size:
        return max_height
    min_ofs_y = min((g["ofs_y"] for g in glyphs if g["h"] > 0), default=0)
    if min_ofs_y > 0:
        for g in glyphs:
            if g["ofs_y"] >= min_ofs_y:
                g["ofs_y"] -= min_ofs_y
    return max(g["ofs_y"] + g["h"] for g in glyphs)


def emit_c(glyphs, font_height, font_size, font_path, out_path, symbol, label,
           copyright=None, license_name=None):
    memory_usage = sum(len(g["bitmap"]) + 7 for g in glyphs)
    lines = []
    lines.append('#include "../rg_gui.h"\n')
    lines.append("\n")
    lines.append("// Generated by tools/gen_cjk_font.py (do not edit by hand)\n")
    lines.append("//\n")
    lines.append("// Source font : %s\n" % os.path.basename(font_path))
    if copyright:
        lines.append("// %s\n" % copyright)
    if license_name:
        lines.append("// Licensed under the %s.\n" % license_name)
        lines.append("//\n")
        lines.append("// The glyph data below is a bitmap rendering of the source\n")
        lines.append("// font, redistributed under the same license. Keep this\n")
        lines.append("// notice if you pass this file on.\n")
    lines.append("// Point size  : %d\n" % font_size)
    lines.append("// Glyph height: %d\n" % font_height)
    lines.append("// Characters  : %d\n" % len(glyphs))
    lines.append("// Memory usage: %d bytes\n" % memory_usage)
    lines.append("//\n")
    lines.append("// Charset: ASCII 0x20-0x7E + GB2312 zone 1/3 (punctuation and\n")
    lines.append("// fullwidth forms) + GB2312 level-1 hanzi (3755 most common),\n")
    lines.append("// minus any code point the source font has no glyph for.\n")
    lines.append("\n")
    lines.append("const rg_font_t %s = {\n" % symbol)
    lines.append('    .name = "%s",\n' % label)
    lines.append("    .type = 1,\n")
    lines.append("    .width = 0,\n")
    lines.append("    .height = %d,\n" % font_height)
    lines.append("    .chars = %d,\n" % len(glyphs))
    lines.append("    .data = {\n")

    for g in glyphs:
        header = (
            g["code"] & 0xFF,
            g["code"] >> 8,
            g["ofs_y"],
            g["w"],
            g["h"],
            g["ofs_x"],
            g["adv"],
        )
        lines.append("        /* U+%04X */ " % g["code"])
        lines.append(", ".join("0x%02X" % b for b in header) + ",")
        if g["bitmap"]:
            lines.append("\n        ")
            blob = [b for b in g["bitmap"]]
            for i in range(0, len(blob), 16):
                lines.append(", ".join("0x%02X" % b for b in blob[i:i + 16]))
                lines.append("," if i + 16 < len(blob) else ",\n")
                if i + 16 < len(blob):
                    lines.append("\n        ")
        else:
            lines.append("\n")

    lines.append("\n        // Terminator\n")
    lines.append("        0x00, 0x00,\n")
    lines.append("    },\n")
    lines.append("};\n")

    with open(out_path, "w", encoding="ascii", newline="\n") as f:
        f.write("".join(lines))


def check_glyph_bounds(glyphs, font_height):
    """
    Refuse to emit a font that rg_gui.c cannot draw safely.

    rg_gui_draw_text() allocates `uint32_t bitmap[gui.font_height]` on the
    stack and writes output[glyph->yOffset + y] for y in 0..height-1, and
    each row is a uint32_t built by shifting the glyph's bits in. So both of
    these are memory corruption at runtime, not cosmetic issues:

        yOffset + height > font height  ->  writes past the row buffer
        xOffset + width  > 32           ->  bits shifted off the uint32_t
    """
    problems = []
    for g in glyphs:
        if g["h"] and g["ofs_y"] + g["h"] > font_height:
            problems.append(
                "U+%04X: yOffset %d + height %d > font height %d"
                % (g["code"], g["ofs_y"], g["h"], font_height))
        if g["ofs_x"] + g["w"] > 32:
            problems.append(
                "U+%04X: xOffset %d + width %d > 32"
                % (g["code"], g["ofs_x"], g["w"]))
    if problems:
        for p in problems[:10]:
            print("  BAD GLYPH: %s" % p)
        sys.exit("refusing to emit a font that would corrupt memory "
                 "(%d bad glyphs)" % len(problems))


def main():
    ap = argparse.ArgumentParser(
        description="Generate a retro-go rg_font_t C source file.")
    ap.add_argument("font_path",
                    help="TTF/TTC to rasterize (required; use a font you may "
                         "redistribute if the output is published)")
    ap.add_argument("font_size", nargs="?", type=int, default=DEFAULT_SIZE,
                    help="pixel size (default: %d)" % DEFAULT_SIZE)
    ap.add_argument("out_path", nargs="?", default=DEFAULT_OUT,
                    help="C file to write (default: %s)" % DEFAULT_OUT)
    ap.add_argument("--symbol", default=DEFAULT_SYMBOL,
                    help="C symbol name (default: %s)" % DEFAULT_SYMBOL)
    ap.add_argument("--label", default=DEFAULT_LABEL,
                    help="name shown in Options -> Font type (default: %s)"
                         % DEFAULT_LABEL)
    ap.add_argument("--copyright", default=None,
                    help="copyright line to repeat in the generated header "
                         "(e.g. 'Copyright (c) 2022, TakWolf')")
    ap.add_argument("--license", default=None,
                    help="license name for the generated header "
                         "(e.g. 'SIL Open Font License 1.1')")
    args = ap.parse_args()

    font_path = args.font_path
    font_size = args.font_size
    out_path = args.out_path

    if not os.path.isfile(font_path):
        sys.exit("font not found: %s" % font_path)

    # index=0 of a .ttc; harmless for plain .ttf.
    pil_font = ImageFont.truetype(font_path, font_size, index=0)
    print("font: %s  (%s)" % (os.path.basename(font_path), " ".join(pil_font.getname())))

    codes = build_charset()
    print("charset: %d code points" % len(codes))

    covered = font_codepoints(font_path)
    if covered is None:
        print("WARNING: fontTools not installed, cannot check glyph coverage")
    else:
        missing = [c for c in codes if c not in covered]
        if missing:
            print("dropping %d code points the font has no glyph for:" % len(missing))
            print("  " + " ".join("U+%04X" % c for c in missing))
        codes = [c for c in codes if c in covered]
        print("charset after coverage filter: %d code points" % len(codes))

    glyphs = []
    for i, code in enumerate(codes):
        glyphs.append(render_glyph(pil_font, code, font_size))
        if (i + 1) % 1000 == 0:
            print("  %d/%d" % (i + 1, len(codes)))

    font_height = trim_top_padding(glyphs, font_size)
    print("glyph height: %d px" % font_height)

    check_glyph_bounds(glyphs, font_height)

    emit_c(glyphs, font_height, font_size, font_path, out_path,
           args.symbol, args.label, args.copyright, args.license)
    total = sum(len(g["bitmap"]) + 7 for g in glyphs) + 2
    print("wrote %s (%d bytes of glyph data)" % (out_path, total))


if __name__ == "__main__":
    main()
