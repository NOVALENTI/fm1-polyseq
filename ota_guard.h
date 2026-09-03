#ifndef OTA_GUARD_H
#define OTA_GUARD_H

#include <stdint.h>

/* ota_guard — bootloader re-entry listener (FLASH SAFETY critical).
 *
 * The FM-1 has no debug pads or recovery buttons: USB MIDI SysEx is the
 * ONLY update channel. Every firmware image shipped from this repo (full
 * AND probe) must feed received USB-MIDI bytes here and honor an upgrade
 * request by jumping back to bootloader/OTA mode. Without this, a flashed
 * device is permanently soft-locked against updates and stock rollback.
 *
 * Protocol (verified: aroum/fm1-custom-fw): frames are
 *   F0 00 32 45 <cmd> <addr/payload...> <crc> F7
 * with cmd 0x01 = verify/meta, 0x02 = start upgrade, 0x03 = data chunk,
 * 0x04 = preset, 0x58 = ACK. Only 0x01/0x02 latch an upgrade request;
 * everything else is parsed and ignored (keeps the parser in sync).
 * Latch is optimistic (at the CMD byte, before frame end): even a
 * truncated upgrade intent is honored — a spurious reboot-to-OTA is
 * always safer than a missed one.
 *
 * INTEGRATION (board glue, not here):
 *   - Call OTA_Guard_FeedByte() for every byte arriving on USB MIDI
 *     (safe from ISR or task context; single-writer assumed per context).
 *   - Poll OTA_Guard_UpgradeRequested() in the main loop; on nonzero,
 *     quiesce audio/sequencer and jump to the bootloader OTA entry.
 *
 * Design: byte-streaming state machine, static state only, no heap,
 * bounded payload skip (OTA_MAX_SKIP), auto-resync on F0. */

#define OTA_CMD_VERIFY   0x01u
#define OTA_CMD_UPGRADE  0x02u
#define OTA_MAX_SKIP     256u  /* max payload bytes skipped per frame */

void OTA_Guard_Init(void);

/* Feed one received byte (or a block) into the parser. */
void OTA_Guard_FeedByte(uint8_t b);
void OTA_Guard_Feed(const uint8_t *buf, uint16_t len);

/* Returns OTA_CMD_VERIFY / OTA_CMD_UPGRADE once per latched request
 * (each call consumes at most one pending request; 0 = none pending). */
uint8_t OTA_Guard_UpgradeRequested(void);

#endif /* OTA_GUARD_H */
