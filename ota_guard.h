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
 * INTEGRATION (board glue, verified against fw-AC79_AIoT_SDK):
 *   - USB device stack (include_lib/driver/device/usb/): select the
 *     composite class with usb_device_mode() (usb_stack.h); USB MIDI is
 *     the Audio-class MIDI-streaming subclass (USB_SUBCLASS_MIDISTREAMING
 *     plus MS IN/OUT JACK descriptors, device/uac_audio.h).
 *   - Register the MIDI bulk-OUT endpoint ISR with usb_g_set_intr_hander()
 *     (usb_stack.h). Inside it, unwrap each 4-byte USB-MIDI event packet
 *     to raw MIDI bytes and call OTA_Guard_FeedByte() per byte — the feed
 *     path is ISR-safe by design (single producer into the SPSC queue).
 *   - Poll OTA_Guard_UpgradeRequested() (or OTA_Dispatch_Poll()) in the
 *     main loop; on nonzero, quiesce audio/sequencer and jump to the
 *     bootloader OTA entry.
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
