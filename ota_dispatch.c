/* ota_dispatch.c — see ota_dispatch.h.
 *
 * Single-writer note: OTA_Guard_FeedByte (USB ISR/task) produces requests,
 * OTA_Dispatch_Poll (main loop) consumes them via the guard's own SPSC
 * queue, so no additional locking is needed here. The quench counter and
 * state are main-loop-private. The jump hook fires exactly once: after it
 * returns (it shouldn't), state stays DONE. */

#include "ota_dispatch.h"
#include "ota_guard.h"

static uint8_t ota_ds_state = OTA_DS_IDLE;
static uint8_t ota_ds_quench = 0u;

void OTA_Dispatch_Init(void)
{
    ota_ds_state = OTA_DS_IDLE;
    ota_ds_quench = 0u;
}

void OTA_Dispatch_Poll(void)
{
    uint8_t req;
    switch (ota_ds_state) {
    case OTA_DS_IDLE:
        req = OTA_Guard_UpgradeRequested();
        if (req != 0u) {
            (void)req; /* verify and upgrade both trigger the handoff */
            OTA_QuenchAudio();
            ota_ds_state = OTA_DS_QUENCH;
            ota_ds_quench = 0u;
        }
        break;
    case OTA_DS_QUENCH:
        /* Drain pending requests so a verify+upgrade burst behind the
         * first frame cannot re-arm the handoff after the jump. */
        while (OTA_Guard_UpgradeRequested() != 0u) {
        }
        ota_ds_quench++;
        if (ota_ds_quench >= OTA_QUENCH_POLLS) {
            ota_ds_state = OTA_DS_DONE;
            OTA_JumpToBootloader();
        }
        break;
    case OTA_DS_DONE:
    default:
        break;
    }
}

uint8_t OTA_Dispatch_State(void)
{
    return ota_ds_state;
}
