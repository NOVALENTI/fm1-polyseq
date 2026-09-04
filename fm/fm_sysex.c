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

#include "fm_sysex.h"

uint8_t FmSysex_ImportVoice(const uint8_t *msg, uint16_t len,
                            uint8_t out_patch[156])
{
    uint16_t i;
    int sum;
    if (msg == 0 || out_patch == 0) {
        return FM_SYSEX_ERR_FRAME;
    }
    if (len != FM_SYSEX_VOICE_LEN) {
        return FM_SYSEX_ERR_LENGTH;
    }
    if (msg[0] != 0xF0u || msg[162] != 0xF7u) {
        return FM_SYSEX_ERR_FRAME;
    }
    /* F0 43 0n 00 01 1B: Yamaha ID, any channel nibble, voice dump. */
    if (msg[1] != 0x43u || (msg[2] & 0xF0u) != 0x00u || msg[3] != 0x00u ||
        msg[4] != 0x01u || msg[5] != 0x1Bu) {
        return FM_SYSEX_ERR_HEADER;
    }
    sum = 0;
    for (i = 0u; i < FM_SYSEX_DATA_LEN; i++) {
        sum -= msg[6u + i];
    }
    if ((uint8_t)(sum & 0x7F) != msg[161]) {
        return FM_SYSEX_ERR_CHECKSUM;
    }
    for (i = 0u; i < FM_SYSEX_DATA_LEN; i++) {
        out_patch[i] = msg[6u + i];
    }
    out_patch[155] = 0x3Fu; /* all operators on (Dexed import behavior) */
    return FM_SYSEX_OK;
}
