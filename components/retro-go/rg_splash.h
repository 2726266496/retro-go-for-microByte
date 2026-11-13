#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Plays the microByte boot animation (the animated plasma + "micro BYTE"
// wordmark of the stock firmware) directly on the LCD.
//
// Blocking, takes roughly 1.5s, and leaves the screen black. Meant to be
// called exactly once, right after rg_display_init(), and only on a cold
// boot - see the call site in rg_system.c.
void rg_splash_play(void);

#ifdef __cplusplus
}
#endif
