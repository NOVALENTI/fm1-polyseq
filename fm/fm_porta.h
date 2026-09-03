/*
 * Copyright 2019 Jean Pierre Cimalando.
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

/* fm_porta.h — C99 port of Dexed/msfa Porta (porta.h/porta.cpp).
 * Per-CC-index portamento step tables (pitch units per FM_N block).
 * Boot-time fill (double); render path is a plain table read. */

#ifndef FM_PORTA_H
#define FM_PORTA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void FmPorta_InitSr(double sample_rate);

/* 128 CC-indexed step tables (Q24 pitch units per block). */
extern int32_t fm_porta_rates[128];
extern int32_t fm_porta_rates_glissando[128];

#ifdef __cplusplus
}
#endif

#endif /* FM_PORTA_H */
