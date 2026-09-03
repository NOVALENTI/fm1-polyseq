/*
 * Copyright 2012 Google Inc.
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

/* fm_pitchenv.h — C99 port of Dexed/msfa PitchEnv (pitchenv.h/pitchenv.cc).
 * DX7 pitch envelope; output Q24/octave, advanced once per FM_N samples.
 * Tables are const (flash); unit_ rate normalization is boot-time. */

#ifndef FM_PITCHENV_H
#define FM_PITCHENV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int rates_[4];
    int levels_[4];
    int32_t level_;
    int targetlevel_;
    int rising_;
    int ix_;
    int inc_;
    int down_;
} FmPitchEnv;

void FmPitchEnv_InitSr(double sample_rate);
void FmPitchEnv_Set(FmPitchEnv *e, const int rates[4], const int levels[4]);
int32_t FmPitchEnv_GetSample(FmPitchEnv *e);
void FmPitchEnv_KeyDown(FmPitchEnv *e, int down);
void FmPitchEnv_GetPosition(FmPitchEnv *e, int8_t *step);

#ifdef __cplusplus
}
#endif

#endif /* FM_PITCHENV_H */
