# This file is injected late into rg_tool.py, you can run arbitrary python code here
# For example override python variables or set environment variables with os.putenv

# Espressif chip in the device
IDF_TARGET = "esp32"

# .fw file format, if supported by the device.
# microByte's stock firmware has no ODROID-GO / ESPlay .fw producer, so we
# deliberately leave this as "none": rg_tool.py's `build-fw` will refuse and
# tell you to use `build-img`, which produces one flat .img flashed at 0x0.
FW_FORMAT = "none"

# Default apps to build when none is specified (comment to build all)
# DEFAULT_APPS = "launcher retro-core"

# ---------------------------------------------------------------------
# Partition sizes.
#
# rg_tool.py defines PROJECT_APPS near the top (lines 20-27) and this file
# is exec()'d at line ~190, i.e. BEFORE build_image() and before
# `apps = [app for app in PROJECT_APPS.keys() if app in apps]` at line 201.
# So overriding the dict here is the supported way to size the partitions,
# but the override must list EVERY app, in the same order, or the omitted
# ones get silently dropped from the image.
#
# Why this override exists: every app links the CJK font (fonts/pixel12.c,
# ~89 KB of glyph table) plus the boot splash. mkfw.py auto-grows a partition
# to the next 64K boundary when an app doesn't fit, which can leave 93-99%
# occupancy - no room for anything else, ever. Fixing the sizes here keeps
# them predictable and leaves ~19-30% headroom. Measured app sizes
# (2026-11-13, full build-img, one CJK font, splash included):
#
#   app           actual .bin   partition    used   free
#   launcher       1,181,616    1,572,864   75.1%  391,248
#   retro-core     1,126,704    1,572,864   71.6%  446,160
#   prboom-go        928,144    1,310,720   70.8%  382,576
#   gwenesis       1,089,952    1,572,864   69.3%  482,912
#   fmsx             753,216    1,048,576   71.8%  295,360
#
# (mkfw prints those floored to whole percent: 75/71/70/69/71. The decimals
#  above are the same numbers, just not floored. Earlier builds that carried
#  a second, 136 KB CJK font sat at 83/80/81/77/84%.)
#
# 0x10000 + 7,077,888 >= a ~6.8 MB image, on a 16 MB chip. Adding GB2312
# level-2 hanzi later (+2892 glyphs, ~+58 KB per app) fits in the slack, as
# does a second 16px CJK font (+~136 KB per app) - but check fmsx first, it
# has the least room.
# If an app ever outgrows its partition mkfw prints
#   "> WARNING: File larger than partition (+N bytes), increased size to ..."
# and still builds - just bump the number here afterwards.
# ---------------------------------------------------------------------
PROJECT_APPS = {
  # Project name  Type, SubType, Size
  'launcher':     [0, 16, 1572864],
  'retro-core':   [0, 16, 1572864],
  'prboom-go':    [0, 16, 1310720],
  'gwenesis':     [0, 16, 1572864],
  'fmsx':         [0, 16, 1048576],
}
