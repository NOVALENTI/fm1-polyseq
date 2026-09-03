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

/* fm_sin.c — C99 port of Dexed/msfa Sin::init/lookup (SIN_DELTA path).
 * Q24 output. Only change from upstream: class -> file-static state. */

#define _USE_MATH_DEFINES
#include <math.h>

#include "fm_sin.h"

/* Own pi constant: M_PI is not standard C99 (hidden under strict ANSI
 * in newlib/glibc alike); boot-time use only. */
#define FM_PI 3.14159265358979323846

#define FM_SIN_R (1 << 29)

static int32_t fm_sintab[FM_SIN_N_SAMPLES << 1];

void FmSin_Init(void)
{
    /* Boot-time only: libm ok here, never in render. */
    double dphase = 2 * FM_PI / FM_SIN_N_SAMPLES;
    int32_t c = (int32_t)floor(cos(dphase) * (1 << 30) + 0.5);
    int32_t s = (int32_t)floor(sin(dphase) * (1 << 30) + 0.5);
    int32_t u = 1 << 30;
    int32_t v = 0;
    int i;
    for (i = 0; i < FM_SIN_N_SAMPLES / 2; i++) {
        fm_sintab[(i << 1) + 1] = (v + 32) >> 6;
        fm_sintab[((i + FM_SIN_N_SAMPLES / 2) << 1) + 1] = -((v + 32) >> 6);
        {
            int32_t t = (int32_t)(((int64_t)u * (int64_t)s +
                                   (int64_t)v * (int64_t)c + FM_SIN_R) >> 30);
            u = (int32_t)(((int64_t)u * (int64_t)c -
                           (int64_t)v * (int64_t)s + FM_SIN_R) >> 30);
            v = t;
        }
    }
    for (i = 0; i < FM_SIN_N_SAMPLES - 1; i++) {
        fm_sintab[i << 1] = fm_sintab[(i << 1) + 3] - fm_sintab[(i << 1) + 1];
    }
    fm_sintab[(FM_SIN_N_SAMPLES << 1) - 2] =
        -fm_sintab[(FM_SIN_N_SAMPLES << 1) - 1];
}

int32_t FmSin_Lookup(int32_t phase)
{
    /* Render path: pure integer (one 32x32->64 MAC for interpolation). */
    const int shift = 24 - FM_SIN_LG_N_SAMPLES;
    int32_t lowbits = phase & ((1 << shift) - 1);
    int32_t phase_int = (phase >> (shift - 1)) & ((FM_SIN_N_SAMPLES - 1) << 1);
    int32_t dy = fm_sintab[phase_int];
    int32_t y0 = fm_sintab[phase_int + 1];

    return y0 + (int32_t)(((int64_t)dy * (int64_t)lowbits) >> shift);
}
