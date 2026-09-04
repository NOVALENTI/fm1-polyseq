/* ota_jump.c — OTA_JumpToBootloader implementation (board glue).
 *
 * Official sequence (SDK apps/common/update/update.c + fs_update.c):
 *   1. update_param_content_fill(): build UPDATA_PARM (CRC + magic).
 *   2. update_param_ram_set(): copy it to UPDATA_FLAG_ADDR
 *      (&UPDATA_BEG + 8, linker-reserved, survives soft reset).
 *   3. system_reset(): CPU resets, UBOOT reads the parm, enters OTA.
 * UPDATA_BEG and system_reset() come from the SDK link (extern here,
 * mocked in host tests, allowlisted in the fw ABI gate as
 * platform-provided). Quiescing (sequencer/audio/DMA stop) happens
 * BEFORE this runs, in ota_dispatch's QUENCH state — this function
 * must not return.
 *
 * Update-type choice: USB_HID_UPDATA matches the stock OTA loader name
 * (usb_hid_ota.bin seen in M-UPGRADE-FM1); wrong-but-valid types safely
 * fall back to normal boot (parm validated by magic+CRC first), so this
 * is confirmable-but-not-bricking on hardware. ota_addr stays 0 pending
 * stock-uboot confirmation (see ota_parm.h). */

#include <stdint.h>

#include "ota_parm.h"

/* From the SDK link (update.h / system): */
extern uint32_t UPDATA_BEG;
extern void system_reset(void);

#ifndef OTA_UPDATE_TYPE
#define OTA_UPDATE_TYPE OTA_TYPE_USB_HID_UPDATA
#endif

#ifndef OTA_LOADER_ADDR
#define OTA_LOADER_ADDR 0u
#endif

void OTA_JumpToBootloader(void)
{
    uint8_t parm[OTA_PARM_LEN];
    uint8_t *flag;
    uint32_t i;

    OTA_BuildParm(parm, (uint16_t)OTA_UPDATE_TYPE,
                  (uint32_t)OTA_LOADER_ADDR);
    flag = (uint8_t *)&UPDATA_BEG + 8u;
    for (i = 0u; i < OTA_PARM_LEN; i++) {
        flag[i] = parm[i];
    }
    system_reset();
    for (;;) {
        /* Must not return; spin if reset somehow didn't take. */
    }
}
