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

/* fm_exp2.h — C99 port of Dexed/msfa Exp2 + Tanh (exp2.h/exp2.cc).
 * Q24 in/out. Inits are boot-time only (libm); lookups pure integer. */

#ifndef FM_EXP2_H
#define FM_EXP2_H

#include <stdint.h>

#define FM_EXP2_LG_N_SAMPLES 10
#define FM_EXP2_N_SAMPLES (1 << FM_EXP2_LG_N_SAMPLES)

#define FM_TANH_LG_N_SAMPLES 10
#define FM_TANH_N_SAMPLES (1 << FM_TANH_LG_N_SAMPLES)

#ifdef __cplusplus
extern "C" {
#endif

void FmExp2_Init(void);
int32_t FmExp2_Lookup(int32_t x);

void FmTanh_Init(void);
int32_t FmTanh_Lookup(int32_t x);

#ifdef __cplusplus
}
#endif

#endif /* FM_EXP2_H */
