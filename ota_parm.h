/* ota_parm.h — UBOOT handoff parameter block builder (pure, host-testable).
 *
 * Mirrors the SDK's update_param_content_fill() single-bank path
 * (fw-AC79_AIoT_SDK apps/common/update/update.c + include_lib/update/
 * update.h, USE_SDFILE_NEW=1 layout, 80-byte struct + 32 priv bytes):
 *   [0:2]   parm_crc   CRC16/XMODEM (poly 0x1021, init 0) over bytes [2..80)
 *   [2:4]   parm_type  UPDATA_TYPE (e.g. USB_HID_UPDATA 0x5A0D)
 *   [4:6]   parm_result UPDATA_READY (0x5A01)
 *   [6:8]   magic      UPDATE_PARAM_MAGIC (0x5441)
 *   [8:40]  file path/patch union (zeros for our flow)
 *   [40:72] parm_priv (zeros)
 *   [72:76] ota_addr   loader addresshint, integration-provided (0 default)
 *   [76:80] ext_arg_len/crc (0)
 *   [80:112] priv tail (zeros; UPDATE_PRIV_PARAM_LEN)
 * UBOOT validates magic + CRC, then enters its OTA loader (verified stock
 * flow; AL-255 analysis). No heap, no I/O, deterministic. */

#ifndef OTA_PARM_H
#define OTA_PARM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_PARM_LEN 112u
#define OTA_PARM_STRUCT_LEN 80u
#define OTA_PARAM_MAGIC 0x5441u
#define OTA_RESULT_READY 0x5A01u

/* UPDATA_TYPE values we may need (SDK update.h, do-not-reorder list). */
#define OTA_TYPE_USB_UPDATA 0x5A00u
#define OTA_TYPE_USB_HID_UPDATA 0x5A0Du

/* CRC16/XMODEM used by the SDK (verified against jl-misctools jl_crc16,
 * itself verified against firmware verifiers). */
uint16_t OTA_Crc16(const uint8_t *data, uint32_t len);

/* Fill the 112-byte block. ota_addr is integration-provided (0 default;
 * confirm against stock uboot behavior on hardware). */
void OTA_BuildParm(uint8_t out[OTA_PARM_LEN], uint16_t type,
                   uint32_t ota_addr);

#ifdef __cplusplus
}
#endif

#endif /* OTA_PARM_H */
