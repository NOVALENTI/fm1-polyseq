/* OTA guard tests: re-entry latching, streaming, resync, overflow. */
#include <stdio.h>
#include <assert.h>

#include "ota_guard.h"

/* Real handshake query from community trace: must NOT trigger. */
static const uint8_t kQuery[] =
    {0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7};
/* Minimal verify + upgrade frames. */
static const uint8_t kVerify[] =
    {0xF0, 0x00, 0x32, 0x45, 0x01, 0x10, 0x20, 0x30, 0xF7};
static const uint8_t kUpgrade[] =
    {0xF0, 0x00, 0x32, 0x45, 0x02, 0xF7};
static const uint8_t kAck[] =
    {0xF0, 0x00, 0x32, 0x45, 0x58, 0x01, 0xF7};
/* Direct OTA upgrade command (AL-255 fm1_ota.py UPGRADE_CMD). */
static const uint8_t kUpgradeDirect[] = {0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7};
/* Near-miss header (third byte wrong): must stay silent. */
static const uint8_t kNearMiss[] = {0xF0, 0x22, 0x24, 0x36, 0x7F, 0xF7};

int main(void)
{
    uint16_t i;

    /* 1. Query + ACK + data-ish frames: silent. */
    OTA_Guard_Init();
    OTA_Guard_Feed(kQuery, (uint16_t)sizeof(kQuery));
    OTA_Guard_Feed(kAck, (uint16_t)sizeof(kAck));
    assert(OTA_Guard_UpgradeRequested() == 0u);

    /* 2. Verify latches 0x01, consumed once. */
    OTA_Guard_Feed(kVerify, (uint16_t)sizeof(kVerify));
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_VERIFY);
    assert(OTA_Guard_UpgradeRequested() == 0u);

    /* 3. Upgrade latches 0x02, byte-at-a-time streaming. */
    OTA_Guard_Init();
    for (i = 0u; i < (uint16_t)sizeof(kUpgrade); i++) {
        OTA_Guard_FeedByte(kUpgrade[i]);
    }
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_UPGRADE);

    /* 4. Back-to-back verify+upgrade keeps both, in order. */
    OTA_Guard_Init();
    OTA_Guard_Feed(kVerify, (uint16_t)sizeof(kVerify));
    OTA_Guard_Feed(kUpgrade, (uint16_t)sizeof(kUpgrade));
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_VERIFY);
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_UPGRADE);
    assert(OTA_Guard_UpgradeRequested() == 0u);

    /* 5. Garbage + wrong-header frames resync to the next valid F0. */
    OTA_Guard_Init();
    {
        const uint8_t garbage[] = {0x99, 0xF0, 0x00, 0x11, 0xF0};
        OTA_Guard_Feed(garbage, (uint16_t)sizeof(garbage));
        assert(OTA_Guard_UpgradeRequested() == 0u);
        OTA_Guard_Feed(kUpgrade, (uint16_t)sizeof(kUpgrade));
        assert(OTA_Guard_UpgradeRequested() == OTA_CMD_UPGRADE);
    }

    /* 6. Oversized payload aborts the frame but the parser survives.
     * Latch is optimistic (at CMD byte): even a truncated upgrade intent
     * is honored; the abort only stops payload consumption. */
    OTA_Guard_Init();
    {
        uint8_t big[OTA_MAX_SKIP + 16u];
        big[0] = 0xF0;
        big[1] = 0x00;
        big[2] = 0x32;
        big[3] = 0x45;
        big[4] = 0x02;
        for (i = 5u; i < (uint16_t)sizeof(big); i++) {
            big[i] = 0x00u;
        }
        OTA_Guard_Feed(big, (uint16_t)sizeof(big));
        assert(OTA_Guard_UpgradeRequested() == OTA_CMD_UPGRADE);
        assert(OTA_Guard_UpgradeRequested() == 0u);
        OTA_Guard_Feed(kVerify, (uint16_t)sizeof(kVerify));
        assert(OTA_Guard_UpgradeRequested() == OTA_CMD_VERIFY);
    }

    /* 7. Direct UPGRADE_CMD latches upgrade; near-miss stays silent;
     * mixed-family bursts keep order. */
    OTA_Guard_Init();
    OTA_Guard_Feed(kNearMiss, (uint16_t)sizeof(kNearMiss));
    assert(OTA_Guard_UpgradeRequested() == 0u);
    OTA_Guard_Feed(kUpgradeDirect, (uint16_t)sizeof(kUpgradeDirect));
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_UPGRADE);
    assert(OTA_Guard_UpgradeRequested() == 0u);
    OTA_Guard_Feed(kVerify, (uint16_t)sizeof(kVerify));
    OTA_Guard_Feed(kUpgradeDirect, (uint16_t)sizeof(kUpgradeDirect));
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_VERIFY);
    assert(OTA_Guard_UpgradeRequested() == OTA_CMD_UPGRADE);
    assert(OTA_Guard_UpgradeRequested() == 0u);

    printf("ALL OTA TESTS PASSED\n");
    return 0;
}
