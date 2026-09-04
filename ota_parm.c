/* ota_parm.c — see ota_parm.h. Integer-only, no libc calls. */

#include "ota_parm.h"

uint16_t OTA_Crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0u;
    uint32_t i;
    if (data == 0) {
        return 0u;
    }
    for (i = 0u; i < len; i++) {
        int k;
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (k = 0; k < 8; k++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

void OTA_BuildParm(uint8_t out[OTA_PARM_LEN], uint16_t type,
                   uint32_t ota_addr)
{
    uint32_t i;
    uint16_t crc;
    if (out == 0) {
        return;
    }
    for (i = 0u; i < OTA_PARM_LEN; i++) {
        out[i] = 0u;
    }
    out[2] = (uint8_t)(type & 0xFFu);
    out[3] = (uint8_t)((type >> 8) & 0xFFu);
    out[4] = (uint8_t)(OTA_RESULT_READY & 0xFFu);
    out[5] = (uint8_t)((OTA_RESULT_READY >> 8) & 0xFFu);
    out[6] = (uint8_t)(OTA_PARAM_MAGIC & 0xFFu);
    out[7] = (uint8_t)((OTA_PARAM_MAGIC >> 8) & 0xFFu);
    out[72] = (uint8_t)(ota_addr & 0xFFu);
    out[73] = (uint8_t)((ota_addr >> 8) & 0xFFu);
    out[74] = (uint8_t)((ota_addr >> 16) & 0xFFu);
    out[75] = (uint8_t)((ota_addr >> 24) & 0xFFu);
    crc = OTA_Crc16(out + 2u, OTA_PARM_STRUCT_LEN - 2u);
    out[0] = (uint8_t)(crc & 0xFFu);
    out[1] = (uint8_t)((crc >> 8) & 0xFFu);
}
