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

/* fm_freqlut.c — C99 port of Dexed/msfa Freqlut::init/lookup.
 * Only change from upstream: class -> file-static state. */

#include <math.h>

#include "fm_freqlut.h"

#define FM_FREQLUT_LG_N 10
#define FM_FREQLUT_N (1 << FM_FREQLUT_LG_N)
#define FM_FREQLUT_SHIFT (24 - FM_FREQLUT_LG_N)
#define FM_FREQLUT_MAXINT 20

static int32_t fm_freqlut[FM_FREQLUT_N + 1];

void FmFreqlut_Init(double sample_rate)
{
    /* Boot-time only: libm ok here, never in render. */
    double y = ((double)(1LL << (24 + FM_FREQLUT_MAXINT))) / sample_rate;
    double inc = pow(2, 1.0 / FM_FREQLUT_N);
    int i;
    for (i = 0; i < FM_FREQLUT_N + 1; i++) {
        fm_freqlut[i] = (int32_t)floor(y + 0.5);
        y *= inc;
    }
}

/* Note: logfreq above 20.0 is inaccurate, but that is many times the
 * Nyquist rate. */
int32_t FmFreqlut_Lookup(int32_t logfreq)
{
    int32_t ix = (logfreq & 0xffffff) >> FM_FREQLUT_SHIFT;
    int32_t y0 = fm_freqlut[ix];
    int32_t y1 = fm_freqlut[ix + 1];
    int32_t lowbits = logfreq & ((1 << FM_FREQLUT_SHIFT) - 1);
    int32_t y = y0 + (int32_t)(((int64_t)(y1 - y0) * (int64_t)lowbits) >>
                               FM_FREQLUT_SHIFT);
    int32_t hibits = logfreq >> 24;

    return y >> (FM_FREQLUT_MAXINT - hibits);
}
