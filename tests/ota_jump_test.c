/* ota_jump_test.c — place-parm + reset sequence with mocked platform.
 * OTA_JumpToBootloader never returns by design; the mock system_reset
 * longjmps back so the test can verify postconditions. */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>

#include "ota_parm.h"
#include "ota_dispatch.h" /* hook declarations (mocked below) */

/* Mock platform: retained-flag RAM window + reset latch. */
uint32_t UPDATA_BEG[64];
static int reset_fired = 0;
static jmp_buf jump_env;

void system_reset(void)
{
    reset_fired++;
    longjmp(jump_env, 1);
}

int main(void)
{
    uint8_t *flag;
    uint8_t expect[OTA_PARM_LEN];

    memset(UPDATA_BEG, 0xAA, sizeof(UPDATA_BEG));
    if (setjmp(jump_env) == 0) {
        OTA_JumpToBootloader();
        assert(0 && "jump must not return");
    }
    assert(reset_fired == 1);

    /* Parm landed at UPDATA_FLAG_ADDR with valid magic + CRC. */
    flag = (uint8_t *)&UPDATA_BEG + 8u;
    assert(flag[2] == 0x0Du && flag[3] == 0x5Au); /* USB_HID_UPDATA */
    assert(flag[6] == 0x41u && flag[7] == 0x54u);
    OTA_BuildParm(expect, OTA_TYPE_USB_HID_UPDATA, 0u);
    assert(memcmp(flag, expect, OTA_PARM_LEN) == 0);

    printf("ALL OTA JUMP TESTS PASSED\n");
    return 0;
}
