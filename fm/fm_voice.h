/*
 * Copyright 2026 FM-1 project (voice manager over ported msfa DSP core).
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

/* fm_voice.h — implements the fm_stub.h contract with the ported engine:
 * 12 static voices (0..5 sequencer, 6..11 live), caller-owned allocation
 * (no stealing across the split), one shared LFO + controllers like the
 * DX7 (single global LFO stepped per 64-sample sub-block, reset on any
 * NoteOn when the patch enables sync — stock DX7/Dexed behavior). */

#ifndef FM_VOICE_H
#define FM_VOICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load a 156-byte DX7 voice dump as the current patch (copied). */
void FM_LoadPatch(const uint8_t patch[156]);

#ifdef __cplusplus
}
#endif

#endif /* FM_VOICE_H */
