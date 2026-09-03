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

/* fm_sin.h — C99 port of Dexed/msfa Sin (sin.h/sin.cc).
 * Interpolated sine LUT, SIN_DELTA variant (8 KB table, static RAM).
 * FmSin_Init is boot-time only (libm); FmSin_Lookup is pure integer. */

#ifndef FM_SIN_H
#define FM_SIN_H

#include <stdint.h>

#define FM_SIN_LG_N_SAMPLES 10
#define FM_SIN_N_SAMPLES (1 << FM_SIN_LG_N_SAMPLES)

/* Fill the table. Call once at boot, before any lookup. */
void FmSin_Init(void);

/* Interpolated sine lookup. Phase is full-circle over 2^24. */
int32_t FmSin_Lookup(int32_t phase);

#endif /* FM_SIN_H */
