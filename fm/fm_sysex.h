/*
 * Copyright 2026 FM-1 project (C99 importer; format per Dexed PluginData).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* fm_sysex.h — DX7 single-voice bulk dump import.
 *
 * Wire format (Dexed PluginData.cpp, standard DX7 voice dump):
 *   F0 43 0n 00 01 1B <155 data bytes> <checksum> F7   (163 bytes)
 * checksum = (-sum(data)) & 0x7F over the 155 data bytes.
 * Internal 156-byte patch = the 155 bytes + byte 155, which is NOT part
 * of the dump: Dexed fills it with 0x3F (all operators on) on import,
 * and so do we. No heap, bounded, no float. */

#ifndef FM_SYSEX_H
#define FM_SYSEX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_SYSEX_VOICE_LEN 163u
#define FM_SYSEX_DATA_LEN 155u

#define FM_SYSEX_OK 0u
#define FM_SYSEX_ERR_FRAME 1u  /* missing F0/F7 envelope */
#define FM_SYSEX_ERR_HEADER 2u /* not a DX7 voice dump header */
#define FM_SYSEX_ERR_LENGTH 3u /* not exactly 163 bytes */
#define FM_SYSEX_ERR_CHECKSUM 4u

/* Validate + unpack a voice dump into the 156-byte patch layout used by
 * FM_LoadPatch. Returns FM_SYSEX_* status. */
uint8_t FmSysex_ImportVoice(const uint8_t *msg, uint16_t len,
                            uint8_t out_patch[156]);

#ifdef __cplusplus
}
#endif

#endif /* FM_SYSEX_H */
