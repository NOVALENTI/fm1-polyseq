/*
 * Copyright 2017 Pascal Gauthier.
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

/* fm_env.h — C99 port of Dexed/msfa Env (env.h/env.cc, ACCURATE_ENVELOPE).
 * DX7 4-stage envelope; output is Q24/doubling log format, advanced once
 * per FM_N samples via FmEnv_GetSample(). Only change from upstream:
 * class -> struct with explicit context pointer. */

#ifndef FM_ENV_H
#define FM_ENV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int initialised_;
    int rates_[4];
    int levels_[4];
    int outlevel_;
    int rate_scaling_;
    int32_t level_;
    int targetlevel_;
    int rising_;
    int ix_;
    int inc_;
    int staticcount_;
    int down_;
} FmEnv;

/* Rates/levels use DX7 0..99 parameterization; outlevel is in microsteps
 * (~0.023 dB, 99*32 = nominal full scale); rate_scaling in qRate (0..63). */
void FmEnv_Init(FmEnv *e, const int rates[4], const int levels[4],
                int outlevel, int rate_scaling);
void FmEnv_Update(FmEnv *e, const int rates[4], const int levels[4],
                  int outlevel, int rate_scaling);
int32_t FmEnv_GetSample(FmEnv *e);
void FmEnv_KeyDown(FmEnv *e, int down);
int FmEnv_ScaleOutLevel(int outlevel);
void FmEnv_GetPosition(FmEnv *e, int8_t *step);
void FmEnv_Transfer(FmEnv *e, const FmEnv *src);
int FmEnv_IsActive(const FmEnv *e);

/* Sample-rate normalization (44100-relative multiplier). Boot-time only. */
void FmEnv_InitSr(double sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* FM_ENV_H */
