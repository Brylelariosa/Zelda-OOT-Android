#pragma once

// C-callable entry point, so the game's C code (padmgr.c) can merge the on-screen controls into the pad it has just read.

#ifdef __ANDROID__

#include <libultraship/libultra/controller.h>

#ifdef __cplusplus
extern "C" {
#endif

// Merges the on-screen touch controls into player 1's pad.
void TouchControls_ReadToPad(OSContPad* pad);

#ifdef __cplusplus
}
#endif

#endif // __ANDROID__
