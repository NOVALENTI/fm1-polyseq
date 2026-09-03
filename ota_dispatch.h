#ifndef OTA_DISPATCH_H
#define OTA_DISPATCH_H

#include <stdint.h>

/* ota_dispatch — upgrade-request handoff (FLASH SAFETY critical).
 *
 * Pairs with ota_guard: the guard DETECTS SysEx 0x01/0x02 frames, this
 * module ACTS on them. Poll OTA_Dispatch_Poll() from the main loop of
 * EVERY firmware image (full and probe). On a latched request it:
 *   1. QUENCH: calls the app hook OTA_QuenchAudio(), then gives the audio
 *      side OTA_QUENCH_POLLS main-loop iterations to drain, so the jump
 *      never leaves stuck notes behind;
 *   2. JUMP: calls the board hook OTA_JumpToBootloader() exactly once,
 *      then parks in DONE (further polls are no-ops).
 *
 * APP HOOK (provided per firmware image):
 *   void OTA_QuenchAudio(void);
 *   Full image: Sequencer_Stop() (pushes AllNotesOff for seq voices).
 *   Probe image: no-op (no sound engine running).
 *
 * BOARD HOOK (provided by board glue, must not return):
 *   void OTA_JumpToBootloader(void);
 * Typical JieLi implementations: watchdog-timeout reset into UBOOT OTA,
 * or set an OTA-magic in retained RAM + system reset. Until the hook can
 * run, the quench alone still leaves the synth silent and stopped.
 *
 * Static state only, no heap, bounded work per poll. */

#define OTA_QUENCH_POLLS 8u

/* Poll states, for debug/telemetry. */
#define OTA_DS_IDLE  0u
#define OTA_DS_QUENCH 1u
#define OTA_DS_DONE  2u

void OTA_Dispatch_Init(void);

/* One main-loop step. Safe to call at any rate; bounded, non-blocking. */
void OTA_Dispatch_Poll(void);

/* Current dispatch state (OTA_DS_*). */
uint8_t OTA_Dispatch_State(void);

#endif /* OTA_DISPATCH_H */
