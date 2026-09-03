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

/* fm_common.h — shared C99 replacements for msfa synth.h utilities:
 * block factor N and min/max (were C++ templates). */

#ifndef FM_COMMON_H
#define FM_COMMON_H

#include <stdint.h>

/* Control-rate factor: envelope/LFO getsample() advances once per N
 * audio samples (matches msfa N = 1 << LG_N, LG_N = 6). */
#define FM_LG_N 6
#define FM_N (1 << FM_LG_N)

static inline int32_t fm_min32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

static inline int32_t fm_max32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static inline int fm_mini(int a, int b)
{
    return a < b ? a : b;
}

#endif /* FM_COMMON_H */
