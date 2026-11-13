# retro-go for microByte

This is [retro-go](https://github.com/ducalex/retro-go) with an added target for the
**microByte** — a small ESP32-WROVER handheld (240×240 panel, SD card, 13 buttons).
It replaces the stock firmware entirely: one `.img` flashed at offset `0x0` contains a
bootloader, a partition table and five applications (launcher + four emulator cores).

Status: builds with ESP-IDF 5.2.1, runs on hardware. Screen, buttons, SD card, audio,
battery gauge, Chinese menu and boot animation were all verified on a real device.
See [Status](#status) for the exact list.

Upstream documents everything else (emulator cores, ROM layout on the SD card,
network features, theming) — read `README.md` and `PORTING.md` from upstream first.

---

## Hardware

| Function | Pin | Notes |
|---|---|---|
| LCD (ST7789, 240×240) | MOSI 13, CLK 14, DC 32, RST 33 | HSPI (SPI2), **no CS pin** (`spics_io_num = -1`) |
| LCD backlight | 15 | LEDC PWM, retro-go's standard backlight driver |
| SD card | MOSI 23, MISO 19, CLK 18, CS 5 | VSPI (SPI3), separate bus from the LCD |
| Buttons | I²C 21/22, TCA9555 @ 0x20 | 13 keys, active low; register layout identical to PCF9539 ⇒ `RG_I2C_GPIO_DRIVER 2`, no new driver code |
| Audio | BCK 27, WS 26, DIN 25 | External I²S DAC (`RG_AUDIO_USE_EXT_DAC 1`), no amp-enable pin |
| Battery | GPIO36 | ADC1 channel 0, `+1310 mV` divider offset, stock's own formula |
| Status LED | GPIO2 | PWM (LEDC high-speed, timer 0, channel 0, 13 bit @ 5 kHz). The only LED the firmware can reach — the charge LED is driven by the charger IC, not by the ESP32 |

The panel is the fussy part. Stock microByte drives it in **SPI mode 3 at 60 MHz** and
sends its tuning commands **after** `SLPOUT` with a 120 ms wait. retro-go's shared
ILI9341 driver used mode 0, 40 MHz, and sent its tuning commands *before* `SLPOUT`;
that combination gave a lit backlight and a completely black screen on this board.
The target now mirrors stock's parameters and init order exactly
(`RG_SCREEN_SPI_MODE`, `RG_SCREEN_SPEED`, and a reordered `RG_SCREEN_INIT()` in
`components/retro-go/targets/microbyte/config.h`).

---

## Changes against upstream retro-go

`diff -rq` against upstream `master`, ignoring build output:

| File | Change |
|---|---|
| `components/retro-go/targets/microbyte/` | **new** — `config.h` (all pins, panel config, battery formula, LED), `env.py` (partition sizes), `sdkconfig` |
| `components/retro-go/config.h` | 2 lines: `#elif defined(RG_TARGET_MICROBYTE)` → include the new target |
| `components/retro-go/drivers/display/ili9341.h` | `RG_SCREEN_SPI_MODE` hook, defaulting to `0`; `.mode = RG_SCREEN_SPI_MODE`. Targets that don't define it compile to byte-identical code |
| `components/retro-go/rg_splash.c/.h`, `splash_sin_table.h`, `bitmaps/image_splash_text.h` | **new** — a faithful port of microByte's stock boot animation (100 procedurally generated plasma frames with the "microBYTE" logo). Plays once on cold boot |
| `components/retro-go/rg_system.c` | calls `rg_splash_play()` after display init on cold boot; sets up the status LED PWM in `platform_init()` and adds `led_pwm_task()` (blinks 250 ms on / 250 ms off while the battery reads ≤ 2%) |
| `components/retro-go/rg_system.h` | one `#include "rg_splash.h"` |
| `components/retro-go/rg_localization.h`, `translations.h` | `RG_LANG_ZH` + the full Chinese translation column (~195 strings) |
| `components/retro-go/fonts/fonts.h`, `fonts/pixel12.c` | **new** CJK font registered as `RG_FONT_PIXEL_12` |
| `tools/gen_cjk_font.py` | **new** — headless generator for the compact `rg_font_t` binary format with CJK coverage (upstream's `font_converter.py` is a tkinter GUI that can only do ASCII/Latin-1) |
| `tools/gen_splash_logo.py` | **new** — generates the boot logo bitmap from a TTF |
| `_idf-build.bat` | Windows convenience wrapper that enters the ESP-IDF environment and runs a `rg_tool.py` command |

### Fonts and Chinese

Upstream's default font (`VeraBold11`) has no CJK glyphs — with it, Chinese text renders
as checkerboard boxes. This target ships one 12 px monospaced CJK bitmap font,
`fonts/pixel12.c`: 4007 code points (ASCII + GB2312 punctuation/fullwidth + the 3755
level-1 hanzi), height 12, ~89 KB, and it is the default font here.
It is generated from **Fusion Pixel Font** (SIL Open Font License 1.1, © 2022 TakWolf);
the generated name is `"Pixel 12"` / `font_pixel12` on purpose, because the upstream
font source carries the OFL Reserved Font Name `Fusion Pixel`, which restricts reuse of
that name. See the header of `fonts/pixel12.c` for the full notice.

Chinese ROM filenames work without any patch: `CONFIG_FATFS_API_ENCODING_UTF_8` gives
`FF_LFN_UNICODE 2` / `MAX_LFN 255`, and retro-go's GUI path is UTF-8 end to end.
The UI language still defaults to English; switch it manually under
`Options → Language`.

To add your own font:

```
python tools/gen_cjk_font.py <font.ttf> 12 components/retro-go/fonts/mine.c \
       --symbol font_mine --label "Mine 12" --license "…"
```

then add one `extern`, one enum value (**append** before `RG_FONT_MAX` — the enum value is
what gets written to the settings file) and one table entry in `fonts/fonts.h`.
Only publish a font file generated from a font you may redistribute.

---

## Building

Requirements: ESP-IDF **5.2.x** (`export.sh` / "ESP-IDF Command Prompt"), Python with
`Pillow` if you want to regenerate fonts or the splash logo. ccache is picked up
automatically and makes rebuilds much faster.

```sh
# Launcher only - fastest way to check that the tree and target are sane (~1.6 MB image)
python rg_tool.py --target=microbyte build-img launcher

# Everything (launcher, retro-core, prboom-go, gwenesis, fmsx) - this is the shipping image
python rg_tool.py --target=microbyte build-img
```

The image lands in the repo root as `retro-go_<ver>_microbyte.img` (`<ver>` is
`git describe`; it is `unknown` in a tree with no tags). On Windows there is a wrapper
that sets up the IDF environment for you:

```
_idf-build.bat python rg_tool.py --target=microbyte build-img
```

Set `IDF_INSTALL_PATH` / `IDF_TOOLS_ID` first if your ESP-IDF is not installed at
`D:\Espressif` with the default tool id (run `idf_cmd_init.bat` with no arguments to
list the ids), or just open an "ESP-IDF Command Prompt" and call it from there.

**There is no `.fw` producer for this device** (`FW_FORMAT = "none"`), so `build-fw`
is not available; use `build-img`, which emits one flat image.

### SD card wear: the `sdmmc_init_spi_crc` patch

Some microByte SD cards reject the CMD59 (CRC on/off) command that IDF's SPI-mode SD
driver issues at init, which makes the card fail to mount (`0x106`).
ESP-IDF 5.2.1 builds fine without a patch **if your card mounts**; if it doesn't, the
usual workaround is to edit IDF's own
`components/sdmmc/sdmmc_sd.c`, in `sdmmc_init_spi_crc()`:

```c
// return err;        /* original */
return ESP_OK;     /* tolerate a card that ignores CMD59 */
```

That is a local modification to ESP-IDF, **not** to this repo — it is not needed to
compile, only to talk to a stubborn card. Prefer fixing the card (or its wiring) if you
can.

### Flashing

The image is written to offset `0x0` and covers the bootloader, the partition table and
all apps:

```
esptool.py --chip esp32 --baud 921600 write_flash 0x0 retro-go_unknown_microbyte.img
```

or the same thing in Espressif's "Flash Download Tool" (start address `0x0`, file
`retro-go_unknown_microbyte.img`). **Back up your stock firmware first** — this replaces
it completely, and the stock firmware needs ESP-IDF 4.1 + GNU Make to rebuild:
`esptool.py read_flash 0 0x1000000 stock_backup.bin`. Note that the Flash Download Tool
can only *write*, it cannot read a backup.

ROMs go on the SD card under `/sd/roms/<system>` (`nes`, `gb`, `sms`, `gg`, `col`, …);
see upstream's `README.md` for the layout each core expects.

---

## Status

Verified on hardware:

* 240×240 panel, correct colours and orientation
* All 13 buttons, plus `SELECT + A` as the virtual OPTION key (microByte has no sixth
  face/menu button; OPTION is a combo here, same compromise as mrgc-gbm)
* SD card mounting and ROM browsing, including Chinese filenames
* Audio through the external I²S DAC
* Battery voltage/percentage
* Chinese UI
* Boot animation
* Status LED on at boot, blinking when the pack is nearly flat

Known limitations / deviations:

* **No self-updater.** `RG_UPDATER_APPLICATION` and `RG_UPDATER_DOWNLOAD_LOCATION` are
  undefined on purpose: `rg_tool.py`'s layout has no `factory` partition, so there is
  nothing to update into. retro-go will still check GitHub for a newer version but will
  not write flash.
* **Partitions are hand-sized.** Two CJK fonts would have pushed every app to 93–99 %
  occupancy (mkfw auto-grows to the next 64 KB), so `targets/microbyte/env.py` overrides
  `PROJECT_APPS` with explicit sizes. If you add an app or a bigger font, adjust the
  numbers there — an app missing from that dict is silently dropped from the image.
* The LED has no hysteresis: hovering around 2 % makes it flip between steady and
  blinking once per second. `RG_LED_PWM_DUTY` (0–8191) and `RG_LED_PWM_BLINK_MS` are in
  the target's `config.h`.
* `targets/microbyte/config.h` still carries a `RG_EXP` panel bring-up selector at the
  top (the single-variable experiment that identified the black-screen cause). Leave it
  at `4`; any other value changes the target name to `MICROBYTE-E<n>` and the panel
  parameters.

---

## License

This fork inherits **GNU GPL v2** from retro-go (see `COPYING`). Emulator cores carry
their own licenses — check each subdirectory before redistributing.

Font and splash data:

* `components/retro-go/fonts/pixel12.c` — rasterized from Fusion Pixel Font,
  SIL Open Font License 1.1, © 2022 TakWolf. Header of that file contains the notice;
  OFL requires that the font data not be sold on its own.
* `components/retro-go/bitmaps/image_splash_text.h` — a 2-bit grey raster of the words
  "micro"/"BYTE" rasterized with `tools/gen_splash_logo.py` from Ubuntu-R.ttf
  (Ubuntu Font Licence 1.0), the font stock microByte used at runtime. Only the bitmap
  is stored here, not the font; supply your own TTF to regenerate it.
* No proprietary font data is included. Microsoft YaHei and similar system fonts must
  never be rasterized into a published font file — see the warning at the top of
  `tools/gen_cjk_font.py`.
