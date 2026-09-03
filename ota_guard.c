/* ota_guard.c — see ota_guard.h.
 *
 * STATE MACHINE (one step per input byte):
 *   IDLE -> F0 -> H0(00) -> H1(32) -> H2(45) -> CMD -> SKIP* -> END(F7)
 * - Any F0 restarts the frame (resync); any status byte >= 0x80 other than
 *   F7/F0 aborts to IDLE (running-status/realtime protection).
 * - CMD latches 0x01/0x02 into a 2-deep pending queue (verify then upgrade
 *   can arrive back-to-back); other commands are consumed silently.
 * - SKIP consumes addr/payload/crc up to OTA_MAX_SKIP bytes, then F7 ends
 *   the frame. Overflow aborts to IDLE (frame ignored, parser survives).
 * - Queue: head/tail indices, volatile for ISR(feed)/main(poll) sharing;
 *   full policy drops the OLDEST (never blocks the feed path). */

#include "ota_guard.h"

#define OTA_QDEPTH 4u /* usable capacity 3: verify+upgrade bursts survive */

static uint8_t ota_state;              /* 0=IDLE 1=H0 2=H1 3=H2 4=CMD 5=SKIP */
static uint16_t ota_skip;              /* payload bytes consumed in SKIP */
static volatile uint8_t ota_q[OTA_QDEPTH];
static volatile uint8_t ota_head = 0u; /* producer (feed) index */
static volatile uint8_t ota_tail = 0u; /* consumer (poll) index */

#define ST_IDLE 0u
#define ST_H0   1u
#define ST_H1   2u
#define ST_H2   3u
#define ST_CMD  4u
#define ST_SKIP 5u

static void ota_push(uint8_t cmd)
{
    uint8_t n = (uint8_t)((ota_head + 1u) % OTA_QDEPTH);
    if (n == ota_tail) {
        ota_tail = (uint8_t)((ota_tail + 1u) % OTA_QDEPTH); /* drop oldest */
    }
    ota_q[ota_head] = cmd;
    ota_head = n;
}

void OTA_Guard_Init(void)
{
    uint8_t i;
    ota_state = ST_IDLE;
    ota_skip = 0u;
    for (i = 0u; i < OTA_QDEPTH; i++) {
        ota_q[i] = 0u;
    }
    ota_head = 0u;
    ota_tail = 0u;
}

void OTA_Guard_FeedByte(uint8_t b)
{
    if (b == 0xF0u) {
        ota_state = ST_H0; /* resync on every SysEx start */
        return;
    }
    switch (ota_state) {
    case ST_H0:
        ota_state = (b == 0x00u) ? ST_H1 : ST_IDLE;
        break;
    case ST_H1:
        ota_state = (b == 0x32u) ? ST_H2 : ST_IDLE;
        break;
    case ST_H2:
        ota_state = (b == 0x45u) ? ST_CMD : ST_IDLE;
        break;
    case ST_CMD:
        if (b == 0xF7u) {
            ota_state = ST_IDLE; /* empty frame */
        } else if (b >= 0x80u) {
            ota_state = ST_IDLE; /* stray status: abort */
        } else {
            if (b == OTA_CMD_VERIFY || b == OTA_CMD_UPGRADE) {
                ota_push(b);
            }
            ota_state = ST_SKIP;
            ota_skip = 0u;
        }
        break;
    case ST_SKIP:
        if (b == 0xF7u) {
            ota_state = ST_IDLE;
        } else if (b >= 0x80u) {
            ota_state = ST_IDLE; /* stray status: abort */
        } else {
            ota_skip++;
            if (ota_skip >= OTA_MAX_SKIP) {
                ota_state = ST_IDLE; /* oversized: drop frame, survive */
            }
        }
        break;
    default:
        ota_state = ST_IDLE;
        break;
    }
}

void OTA_Guard_Feed(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    if (buf == 0) {
        return;
    }
    for (i = 0u; i < len; i++) {
        OTA_Guard_FeedByte(buf[i]);
    }
}

uint8_t OTA_Guard_UpgradeRequested(void)
{
    uint8_t cmd = 0u;
    if (ota_head != ota_tail) {
        cmd = ota_q[ota_tail];
        ota_tail = (uint8_t)((ota_tail + 1u) % OTA_QDEPTH);
    }
    return cmd;
}
