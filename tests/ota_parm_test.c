/* ota_parm_test.c — CRC oracle vectors (jl-misctools jl_crc16) + layout. */
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "ota_parm.h"

int main(void)
{
    uint8_t buf[OTA_PARM_LEN];
    uint8_t seq[256];
    uint32_t i;

    /* CRC oracle vectors. */
    assert(OTA_Crc16((const uint8_t *)"", 0u) == 0x0000u);
    assert(OTA_Crc16((const uint8_t *)"123456789", 9u) == 0x31C3u);
    for (i = 0u; i < 256u; i++) {
        seq[i] = (uint8_t)i;
    }
    assert(OTA_Crc16(seq, 256u) == 0x7E55u);
    {
        uint8_t parm_like[80];
        memset(parm_like, 0, sizeof(parm_like));
        parm_like[0] = 0x5A;
        parm_like[2] = 0x01;
        parm_like[3] = 0x5A;
        parm_like[4] = 0x41;
        parm_like[5] = 0x54;
        assert(OTA_Crc16(parm_like, 80u) == 0xA41Fu);
    }
    assert(OTA_Crc16(0, 10u) == 0u);

    /* Layout: magic/type/result/offsets, CRC self-consistency. */
    memset(buf, 0xAA, sizeof(buf));
    OTA_BuildParm(buf, OTA_TYPE_USB_HID_UPDATA, 0x1C0A800u);
    assert(buf[2] == 0x0Du && buf[3] == 0x5Au); /* type LE */
    assert(buf[4] == 0x01u && buf[5] == 0x5Au); /* UPDATA_READY */
    assert(buf[6] == 0x41u && buf[7] == 0x54u); /* magic 0x5441 */
    assert(buf[72] == 0x00u && buf[73] == 0xA8u);
    assert(buf[74] == 0xC0u && buf[75] == 0x01u); /* ota_addr LE */
    for (i = 8u; i < 72u; i++) {
        assert(buf[i] == 0u);
    }
    for (i = 76u; i < OTA_PARM_LEN; i++) {
        assert(buf[i] == 0u);
    }
    {
        uint16_t crc = OTA_Crc16(buf + 2u, OTA_PARM_STRUCT_LEN - 2u);
        assert(buf[0] == (uint8_t)(crc & 0xFFu));
        assert(buf[1] == (uint8_t)((crc >> 8) & 0xFFu));
    }
    /* Deterministic + null-safe. */
    {
        uint8_t again[OTA_PARM_LEN];
        OTA_BuildParm(again, OTA_TYPE_USB_HID_UPDATA, 0x1C0A800u);
        assert(memcmp(buf, again, sizeof(buf)) == 0);
        OTA_BuildParm(0, 0u, 0u);
    }

    printf("ALL OTA PARM TESTS PASSED\n");
    return 0;
}
