/**
 * =====================================================================
 *  retro-go target: MICROBYTE
 *  Hardware: ESP32-WROVER, 240x240 ST7789 (HSPI), TCA9555 I2C buttons,
 *            SD card on VSPI, external I2S DAC, 16MB flash.
 * ---------------------------------------------------------------------
 *  Every value here was derived from microByte_firmware's own drivers and
 *  confirmed on hardware (screen, buttons, SD, audio, battery, Chinese UI,
 *  boot animation). See MICROBYTE.md at the repo root for the pin map and
 *  the list of changes against upstream retro-go.
 *
 *  This target is selected by RG_TARGET=microbyte, which also sets
 *  RG_TARGET_MICROBYTE; components/retro-go/config.h includes this file
 *  for that define.
 * =====================================================================
 */

#pragma once

/**
 * =====================================================================
 *  临时：面板点亮实验选择器（2026-11-12，见 README §12 / 点亮测试说明.txt）
 * ---------------------------------------------------------------------
 *  11 号那份镜像背光亮、画面全黑。已知"原厂能亮"，所以嫌疑集中在三个
 *  互相独立的因素上：
 *      order = RG_SCREEN_INIT() 里那串调参命令的**顺序/延时**
 *      mode  = SPI 模式（0 还是 3）
 *      speed = SPI 时钟（40MHz 还是 60MHz）
 *  fix1 一次把三项全上了。这份选择器让每一项单独可测：
 *
 *   RG_EXP  order      mode  speed  说明
 *   ------------------------------------------------------------------
 *     0    OLD(旧序)   0     40M   11/11 黑屏那份的配置（回归对照）
 *     1    NEW(原厂序) 0     40M   只改时序
 *     2    OLD         3     40M   只改 mode
 *     3    OLD         0     60M   只改 speed
 *     4    NEW         3     60M   = fix1（默认，五项全上那版）
 *     5    NEW         3     40M   时序 + mode
 *     6    NEW         0     60M   时序 + speed
 *     7    MIN(最小序) 3     60M   只发 SWRESET/SLPOUT/MADCTL/COLMOD/
 *                                   DISPON，一个调参都不发 → 用来判断
 *                                   "是不是我们那串调参本身把面板弄黑了"
 *     8    OLD         3     60M   mode+speed，仍用旧序（fix1 的反向单变量）
 *
 *  编译时改这一行的数字（或用 sed），编出来的镜像里
 *  Options -> About 的 "Target" 会显示 MICROBYTE-En，
 *  串口日志开头也会打 `built for: MICROBYTE-En`，
 *  刷之前/之后都能确认手上这份到底是哪一版。
 *  RG_EXP 保持 4 时 Target 就是普通的 "MICROBYTE"，与 fix1 完全一致。
 * ===================================================================== */
#ifndef RG_EXP
#define RG_EXP 4
#endif

#define RG_STRINGIFY_(x) #x
#define RG_STRINGIFY(x) RG_STRINGIFY_(x)

#if RG_EXP == 4
#define RG_TARGET_NAME "MICROBYTE"
#else
#define RG_TARGET_NAME "MICROBYTE-E" RG_STRINGIFY(RG_EXP)
#endif

/**
 * ---------------------------------------------------------------------
 *  Storage  --  microByte wires the SD card to VSPI (SPI3) and the LCD to
 *  HSPI (SPI2), so the two never share a bus.
 *  microByte_firmware/components/drivers/system_configuration/system_configuration.h
 *      VSPI_MOSI 23, VSPI_MISO 19, VSPI_CLK 18, VSPI_CS0 5
 * --------------------------------------------------------------------- */
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI3_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_19
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_23
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_18
#define RG_GPIO_SDSPI_CS            GPIO_NUM_5

/**
 * ---------------------------------------------------------------------
 *  Buttons  --  TCA9555 at 0x20, register layout identical to PCF9539,
 *  which retro-go already supports as RG_I2C_GPIO_DRIVER 2:
 *      input 0x00/0x01, output 0x02/0x03, config 0x06/0x07
 *  => zero new driver code.
 *
 *  Bit layout is taken verbatim from
 *  components/drivers/user_input/user_input_HAL/user_input.h
 *      0 Down    1 Left    2 Up     3 Right
 *      5 R       6 X       7 Y      8 B      9 A
 *      10 Start  11 Menu   12 Select  13 L
 *  TCA9555 inputs are active-low, so .level = 0 on every entry.
 * --------------------------------------------------------------------- */
#define RG_GAMEPAD_I2C_MAP {\
    {RG_KEY_UP,     .num = 2,  .level = 0},\
    {RG_KEY_RIGHT,  .num = 3,  .level = 0},\
    {RG_KEY_DOWN,   .num = 0,  .level = 0},\
    {RG_KEY_LEFT,   .num = 1,  .level = 0},\
    {RG_KEY_SELECT, .num = 12, .level = 0},\
    {RG_KEY_START,  .num = 10, .level = 0},\
    {RG_KEY_MENU,   .num = 11, .level = 0},\
    {RG_KEY_A,      .num = 9,  .level = 0},\
    {RG_KEY_B,      .num = 8,  .level = 0},\
    {RG_KEY_X,      .num = 6,  .level = 0},\
    {RG_KEY_Y,      .num = 7,  .level = 0},\
    {RG_KEY_L,      .num = 13, .level = 0},\
    {RG_KEY_R,      .num = 5,  .level = 0},\
}

/* microByte has 13 physical buttons and no dedicated OPTION key.
   Same compromise as mrgc-gbm / esplay-micro / retro-esp32: a combo.
   REVIEW: if it interferes with games, delete this block and OPTION
   simply won't exist (the menus degrade gracefully). */
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_OPTION, .src = RG_KEY_SELECT | RG_KEY_A},\
}

#define RG_I2C_GPIO_DRIVER          2 /* PCF9539 == TCA9555 */
#define RG_I2C_GPIO_ADDR            0x20
#define RG_GPIO_I2C_SDA             GPIO_NUM_21
#define RG_GPIO_I2C_SCL             GPIO_NUM_22

/**
 * ---------------------------------------------------------------------
 *  Audio  --  microByte drives an EXTERNAL I2S DAC (not the ESP32 internal
 *  DAC): sound_driver.c installs I2S_MODE_MASTER|I2S_MODE_TX with
 *  I2S_COMM_FORMAT_I2S|I2S_COMM_FORMAT_I2S_MSB on I2S_NUM_0, with
 *  BCK=27 WS=26 DATA=25. Same shape as mrgc-gbm.
 *  There is no amplifier-enable pin on this board.
 * --------------------------------------------------------------------- */
#define RG_AUDIO_USE_INT_DAC        0
#define RG_AUDIO_USE_EXT_DAC        1
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_27
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_26
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_25

/**
 * ---------------------------------------------------------------------
 *  Display  --  240x240 ST7789 on HSPI (SPI2), no CS pin, hardware RST.
 *  This target now mirrors microByte's stock driver as closely as
 *  retro-go allows: SPI mode 3, 60MHz, and the same init order/timings
 *  (the "fix1" bring-up -- see RG_SCREEN_INIT below).
 *  RG_SCREEN_SPI_MODE is consumed by the shared ili9341.h driver, which
 *  defaults it to 0 for every other target.
 * --------------------------------------------------------------------- */
#define RG_SCREEN_DRIVER            0 /* ILI9341 + ST7789 share this driver */
#define RG_SCREEN_HOST              SPI2_HOST
/* IDF's spi_master.h only defines 80/40/26/20/10M, no 60M, so the 60M figure
   below is the literal microByte's own HSPI_CLK_SPEED uses (60*1000*1000).
   The driver picks the closest achievable divider below it (~53MHz on ESP32).
   (RG_EXP 0/1/2/5 keep the pre-fix1 40M.) */
#if (RG_EXP == 0) || (RG_EXP == 1) || (RG_EXP == 2) || (RG_EXP == 5)
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M
#else
#define RG_SCREEN_SPEED             60000000
#endif
#if (RG_EXP == 2) || (RG_EXP == 4) || (RG_EXP == 5) || (RG_EXP == 7) || (RG_EXP == 8)
#define RG_SCREEN_SPI_MODE          3 /* stock ST7789_driver.c: .mode = 3 */
#else
#define RG_SCREEN_SPI_MODE          0 /* retro-go's long-standing default */
#endif
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             240
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}   /* microByte addresses the full 0..239 x 0..239 */
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}

/* Defining BOTH makes lcd_init() emit MADCTL 0x00, which is exactly what
   microByte sends (ST7789_driver.c init sequence: MADCTL \x00). */
#define RG_SCREEN_ROTATION          0
#define RG_SCREEN_RGB_BGR           0

#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_13
#define RG_GPIO_LCD_CLK             GPIO_NUM_14
#define RG_GPIO_LCD_CS              GPIO_NUM_NC   /* microByte uses spics_io_num = -1 */
#define RG_GPIO_LCD_DC              GPIO_NUM_32
#define RG_GPIO_LCD_BCKL            GPIO_NUM_15
#define RG_GPIO_LCD_RST             GPIO_NUM_33

/**
 *  Init sequence transcribed from microByte's known-good ST7789_config()
 *  (components/drivers/display/ST7789/ST7789_driver.c:250-298), replayed
 *  in the SAME ORDER and with the SAME TIMINGS as the stock driver, since
 *  that is the only combination this panel is known to accept:
 *
 *      stock (ST7789_config)                  this macro
 *      -------------------------------------  ----------------------------
 *      RST low 20ms, high, wait 130ms         vTaskDelay(150ms) lead-in
 *      SLPIN 10ms, SWRESET 200ms              (done by lcd_init already)
 *      SLPOUT 120ms                           ILI9341_CMD(0x11) + 120ms
 *      MADCTL, COLMOD, INVON, tuning          MADCTL, COLMOD, INVON, tuning
 *      RAMCTRL 0x00 0xc8                      RAMCTRL 0x00 0xc0 (see below)
 *      black fill, DISPON 100ms               ILI9341_CMD(0x29)
 *      SLPOUT 100ms, CASET/RASET/RAMWR        (lcd_init's trailing 0x11/0x29)
 *
 *  Everything lcd_init() sends before this  --  SWRESET, 5ms, COLMOD,
 *  MADCTL  --  and the 0x11 / 10ms / 0x29 it sends after are harmless:
 *  the trailing pair is a no-op on an already-awake, already-on panel.
 *  The 150ms lead-in is what stock gets from ST7789_reset()'s "RST high,
 *  wait 130ms" before its first command; lcd_init() waits only 10ms.
 *
 *  Kept:    INVON, PORCTRL, GCTRL, VCOMS, VDVVRHEN, VRHSET, VDVSET,
 *           PWCTRL1, FRCTR2, GAMSET, PVGAMCTRL, NVGAMCTRL, RAMCTRL.
 *
 *  RAMCTRL is deliberately 0x00 0xC0. microByte's own init writes
 *  0x00 0xC8 (labelled "Little endian") but main.c:148 later calls
 *  display_HAL_change_endian(), which rewrites it to 0x00 0xC0  --  and
 *  that is the state the panel is in for the whole emulator session.
 *  0xC0 is also the ST7789 power-on default and what mrgc-gbm relies on.
 *  REVIEW: if colours come out byte-swapped, change to 0xC8 here  --  this
 *  is the single most likely tweak.
 */
/* ---------------------------------------------------------------------
 *  RG_SCREEN_INIT() 三个版本，由文件最上面的 RG_EXP 选一个：
 *      0/2/3/8 -> OLD（旧序，调参在 SLPOUT 之前）
 *      7       -> MIN（最小命令集）
 *      其它     -> NEW（原厂顺序 = fix1）
 *  三份的**调参值完全一样**，差别只在"什么时候发"和"发不发"。
 * --------------------------------------------------------------------- */
#if (RG_EXP == 0) || (RG_EXP == 2) || (RG_EXP == 3) || (RG_EXP == 8)

#define RG_SCREEN_INIT()                                            \
    ILI9341_CMD(0x21);                    /* INVON  (inversion on)  */ \
    ILI9341_CMD(0xB2, 0x0c, 0x0c, 0x00, 0x33, 0x33); /* PORCTRL     */ \
    ILI9341_CMD(0xB7, 0x14);              /* GCTRL                  */ \
    ILI9341_CMD(0xBB, 0x37);              /* VCOMS                  */ \
    ILI9341_CMD(0xC2, 0x01, 0xff);        /* VDVVRHEN               */ \
    ILI9341_CMD(0xC3, 0x12);              /* VRHSET                 */ \
    ILI9341_CMD(0xC4, 0x20);              /* VDVSET                 */ \
    ILI9341_CMD(0xD0, 0xa4, 0xa1);        /* PWCTRL1                */ \
    ILI9341_CMD(0xC6, 0x0f);              /* FRCTR2 (60 fps)        */ \
    ILI9341_CMD(0x26, 0x01);              /* GAMSET (gamma 2.2)     */ \
    ILI9341_CMD(0xE0, 0xd0, 0x08, 0x11, 0x08, 0x0c, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2d); \
    ILI9341_CMD(0xE1, 0xd0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0b, 0x16, 0x14, 0x2f, 0x31); \
    ILI9341_CMD(0xB0, 0x00, 0xc0);        /* RAMCTRL (see above)    */


#elif RG_EXP == 7

/* 最小初始化：只发最小的命令集，一个调参都不发。
   ST7789 上电默认值就能出图；如果这一版亮了、NEW 那份还是黑，
   说明黑屏是那串调参（PORCTRL/VRHSET/gamma 等）本身造成的。
   颜色可能是反的（没有 INVON），那是预期的，只要能看到画面就算过。 */
#define RG_SCREEN_INIT()                                              \
    vTaskDelay(pdMS_TO_TICKS(150));       /* 对齐原厂 RST 后 130ms   */ \
    ILI9341_CMD(0x01);                    /* SWRESET                  */ \
    vTaskDelay(pdMS_TO_TICKS(200));       /* 原厂 SWRESET 后 200ms    */ \
    ILI9341_CMD(0x11);                    /* SLPOUT (exit sleep)      */ \
    vTaskDelay(pdMS_TO_TICKS(120));       /* 原厂 SLPOUT 后 120ms     */ \
    ILI9341_CMD(0x36, 0x00);              /* MADCTL                   */ \
    ILI9341_CMD(0x3A, 0x55);              /* COLMOD (16-bit RGB)      */ \
    ILI9341_CMD(0x29);                    /* DISPON (display on)      */


#else /* RG_EXP == 1, 4, 5, 6 -> 原厂顺序（fix1） */

#define RG_SCREEN_INIT()                                              \
    vTaskDelay(pdMS_TO_TICKS(150));       /* stock: RST high + 130ms */ \
    ILI9341_CMD(0x11);                    /* SLPOUT (exit sleep)     */ \
    vTaskDelay(pdMS_TO_TICKS(120));       /* stock: 120ms after SLPOUT */ \
    ILI9341_CMD(0x36, 0x00);              /* MADCTL                  */ \
    ILI9341_CMD(0x3A, 0x55);              /* COLMOD (16-bit RGB)     */ \
    ILI9341_CMD(0x21);                    /* INVON  (inversion on)  */ \
    ILI9341_CMD(0xB2, 0x0c, 0x0c, 0x00, 0x33, 0x33); /* PORCTRL     */ \
    ILI9341_CMD(0xB7, 0x14);              /* GCTRL                  */ \
    ILI9341_CMD(0xBB, 0x37);              /* VCOMS                  */ \
    ILI9341_CMD(0xC2, 0x01, 0xff);        /* VDVVRHEN               */ \
    ILI9341_CMD(0xC3, 0x12);              /* VRHSET                 */ \
    ILI9341_CMD(0xC4, 0x20);              /* VDVSET                 */ \
    ILI9341_CMD(0xD0, 0xa4, 0xa1);        /* PWCTRL1                */ \
    ILI9341_CMD(0xC6, 0x0f);              /* FRCTR2 (60 fps)        */ \
    ILI9341_CMD(0x26, 0x01);              /* GAMSET (gamma 2.2)     */ \
    ILI9341_CMD(0xE0, 0xd0, 0x08, 0x11, 0x08, 0x0c, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2d); \
    ILI9341_CMD(0xE1, 0xd0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0b, 0x16, 0x14, 0x2f, 0x31); \
    ILI9341_CMD(0xB0, 0x00, 0xc0);        /* RAMCTRL (see above)    */ \
    ILI9341_CMD(0x29);                    /* DISPON (display on)    */

#endif

/**
 * ---------------------------------------------------------------------
 *  Battery  --  microByte reads GPIO36 on ADC1 with a 1040mV calibration
 *  reference (battery.c) and then adds a 1310mV divider offset.
 *  retro-go's ADC battery driver hardcodes a 1100mV reference, so the
 *  (1040/1100) factor below re-derives microByte's own number.
 *
 *  microByte's maths, verbatim:
 *      voltage  = esp_adc_cal_raw_to_voltage(raw, &chars) + 1310;
 *      percent  = (voltage - 3570.6) / 5.2128;
 *      if (voltage < 3576) percent = 1;      // floor
 *      if (percent > 100)  percent = 100;    // ceiling
 *
 *  retro-go already clamps the result into 0..100, so only the 1% floor
 *  is dropped here (cosmetic).
 * --------------------------------------------------------------------- */
#define RG_BATTERY_DRIVER           1 /* ADC */
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_0 /* GPIO36 */
#define RG_BATTERY_CALC_PERCENT(raw) \
    (((raw) * (1040.f / 1100.f) + 1310.f - 3570.6f) / 5.2128f)
#define RG_BATTERY_CALC_VOLTAGE(raw) \
    (((raw) * (1040.f / 1100.f) + 1310.f) * 0.001f)

/**
 * ---------------------------------------------------------------------
 *  Status LED  --  microByte_firmware/system_configuration.h: LED_PIN 2.
 *
 *  The stock firmware drives it as PWM (components/drivers/LED/LED_notification.c:
 *  LEDC_HIGH_SPEED_MODE, timer 0, channel 0, 13 bit @ 5 kHz, duty 5000 = ~61%),
 *  fades it on at boot and leaves it on. So we don't use RG_GPIO_LED (retro-go's
 *  on/off disk-activity indicator) here: that would only blink on SD access and
 *  fight the PWM. Instead the target asks rg_system.c's platform_init() to set up
 *  the same LEDC channel and turn it on.
 *
 *  Note: with RG_GPIO_LED undefined, Options -> LED options no longer appears
 *  (that menu configures the on/off indicator that we're not using).
 *
 *  Low battery warning: the LED stays on at RG_LED_PWM_DUTY, and blinks
 *  (250ms on / 250ms off) while the battery reads <= 2%, i.e. while
 *  rg_system.c's system_monitor_task has RG_INDICATOR_POWER_LOW set. That is
 *  stock's LED_BLINK_HS rhythm; stock never actually enabled it, so this is a
 *  small addition rather than a reimplementation of something you'd seen.
 *  It is done with plain duty writes (no ledc fade), so it cannot interfere
 *  with the backlight's fade setup in the LOW speed mode.
 * --------------------------------------------------------------------- */
// #define RG_GPIO_LED              GPIO_NUM_2
#define RG_LED_PWM_GPIO             GPIO_NUM_2
#define RG_LED_PWM_DUTY             5000  /* stock: 5000 of 8191 (13 bit) */
#define RG_LED_PWM_LOWBAT_BLINK     1     /* blink while battery <= 2% */
#define RG_LED_PWM_BLINK_MS         250   /* stock LED_BLINK_HS: 250 on / 250 off */

/**
 * ---------------------------------------------------------------------
 *  Updater  --  intentionally left as VERSION-CHECK-ONLY.
 *  rg_tool.py's image layout has no `factory` partition; apps land in
 *  ota_0..ota_N, so there is nothing to self-update into. With both
 *  RG_UPDATER_APPLICATION and RG_UPDATER_DOWNLOAD_LOCATION undefined,
 *  retro-go checks GitHub for a newer release but will not write flash.
 *  Re-enable only after building a proper partitioning.
 * --------------------------------------------------------------------- */
#define RG_UPDATER_ENABLE           1

/**
 * ---------------------------------------------------------------------
 *  Font  --  one CJK font is compiled in and used as the default.
 *
 *  retro-go's stock default (VeraBold11) has no CJK glyphs at all, so
 *  selecting Chinese with it draws every hanzi as a checkerboard box.
 *  RG_FONT_PIXEL_12 covers ASCII + GB2312 punctuation/fullwidth + the
 *  3755 level-1 hanzi, so English and Chinese both render.
 *
 *    RG_FONT_PIXEL_12  "Pixel 12"  4007 glyphs, height 12, 89KB
 *        A 12px monospaced CJK bitmap font. At size 12 its rasterization
 *        is exactly binary (zero antialiased pixels, identical to a 24px
 *        render downscaled 2x), i.e. it sits on its own pixel grid, and
 *        12px matches the apparent size of retro-go's VeraBold11 (13px),
 *        so the menu layout is unchanged - stock microByte's own UI uses
 *        LVGL Montserrat 12/20 on this same panel.
 *
 *        Data generated with tools/gen_cjk_font.py from Fusion Pixel Font
 *        (SIL Open Font License 1.1, (c) 2022 TakWolf). The font name was
 *        deliberately changed to "Pixel 12" / font_pixel12: the upstream
 *        source repo carries the OFL Reserved Font Name "Fusion Pixel",
 *        which restricts reuse of that name. See fonts/pixel12.c for the
 *        full notice and OFL text.
 *
 *  Switch at Options -> Font type (cycles through every RG_FONT_MAX
 *  entry). Picking any non-CJK font loses Chinese.
 *  Language defaults to English on purpose: switch it under
 *  Options -> Language.
 * --------------------------------------------------------------------- */
#define RG_FONT_DEFAULT             RG_FONT_PIXEL_12
