/*
 * Copyright 2013 Google Inc.
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

/* fm_lfo.h — C99 port of Dexed/msfa Lfo (lfo.h/lfo.cc), DX7-compatible.
 * One deliberate defined-behavior fix: upstream leaves phase_/randstate_
 * uninitialized (implicit ctor); FmLfo_Reset zeroes them. With keydown
 * sync the trajectories match upstream exactly; sample&hold from a cold
 * start is therefore deterministic here (unspecified there). */

#ifndef FM_LFO_H
#define FM_LFO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t phase_;
    uint32_t delta_;
    uint8_t waveform_;
    uint8_t randstate_;
    int sync_;
    uint32_t delaystate_;
    uint32_t delayinc_;
    uint32_t delayinc2_;
} FmLfo;

/* Rate table + phase quantum setup. Boot-time only (double). */
void FmLfo_Init(double sample_rate);

/* params[6]: DX7 LFO block (rate, delay, ..., sync, waveform). */
void FmLfo_Reset(FmLfo *l, const uint8_t params[6]);

/* 0..1 in Q24, advanced once per FM_N samples. */
int32_t FmLfo_GetSample(FmLfo *l);
int32_t FmLfo_GetDelay(FmLfo *l);
void FmLfo_KeyDown(FmLfo *l);

#ifdef __cplusplus
}
#endif

#endif /* FM_LFO_H */
