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

/* fm_exp2.c — C99 port of Dexed/msfa Exp2/Tanh init+lookup.
 * Only change from upstream: classes -> file-static state. */

#define _USE_MATH_DEFINES
#include <math.h>

#include "fm_exp2.h"

#ifdef _MSC_VER
#define exp2(arg) pow(2.0, arg)
#endif

static int32_t fm_exp2tab[FM_EXP2_N_SAMPLES << 1];
static int32_t fm_tanhtab[FM_TANH_N_SAMPLES << 1];

void FmExp2_Init(void)
{
    /* Boot-time only: libm ok here, never in render. */
    double inc = exp2(1.0 / FM_EXP2_N_SAMPLES);
    double y = 1 << 30;
    int i;
    for (i = 0; i < FM_EXP2_N_SAMPLES; i++) {
        fm_exp2tab[(i << 1) + 1] = (int32_t)floor(y + 0.5);
        y *= inc;
    }
    for (i = 0; i < FM_EXP2_N_SAMPLES - 1; i++) {
        fm_exp2tab[i << 1] = fm_exp2tab[(i << 1) + 3] - fm_exp2tab[(i << 1) + 1];
    }
    fm_exp2tab[(FM_EXP2_N_SAMPLES << 1) - 2] =
        (int32_t)((1U << 31) - (uint32_t)fm_exp2tab[(FM_EXP2_N_SAMPLES << 1) - 1]);
}

int32_t FmExp2_Lookup(int32_t x)
{
    const int shift = 24 - FM_EXP2_LG_N_SAMPLES;
    int32_t lowbits = x & ((1 << shift) - 1);
    int32_t x_int = (x >> (shift - 1)) & ((FM_EXP2_N_SAMPLES - 1) << 1);
    int32_t dy = fm_exp2tab[x_int];
    int32_t y0 = fm_exp2tab[x_int + 1];
    int32_t y = y0 + (int32_t)(((int64_t)dy * (int64_t)lowbits) >> shift);

    return y >> (6 - (x >> 24));
}

static double fm_dtanh(double y)
{
    return 1 - y * y;
}

void FmTanh_Init(void)
{
    /* Boot-time only: 4th-order Runge-Kutta on tanh's diffeq. */
    double step = 4.0 / FM_TANH_N_SAMPLES;
    double y = 0;
    int i;
    int32_t lasty;
    for (i = 0; i < FM_TANH_N_SAMPLES; i++) {
        double k1, k2, k3, k4, dy;
        fm_tanhtab[(i << 1) + 1] = (int32_t)((1 << 24) * y + 0.5);
        k1 = fm_dtanh(y);
        k2 = fm_dtanh(y + 0.5 * step * k1);
        k3 = fm_dtanh(y + 0.5 * step * k2);
        k4 = fm_dtanh(y + step * k3);
        dy = (step / 6) * (k1 + k4 + 2 * (k2 + k3));
        y += dy;
    }
    for (i = 0; i < FM_TANH_N_SAMPLES - 1; i++) {
        fm_tanhtab[i << 1] = fm_tanhtab[(i << 1) + 3] - fm_tanhtab[(i << 1) + 1];
    }
    lasty = (int32_t)((1 << 24) * y + 0.5);
    fm_tanhtab[(FM_TANH_N_SAMPLES << 1) - 2] =
        lasty - fm_tanhtab[(FM_TANH_N_SAMPLES << 1) - 1];
}

int32_t FmTanh_Lookup(int32_t x)
{
    int32_t signum = x >> 31;
    x ^= signum;
    if (x >= (4 << 24)) {
        if (x >= (17 << 23)) {
            return signum ^ (1 << 24);
        } else {
            int32_t sx = (int32_t)(((int64_t)-48408812 * (int64_t)x) >> 24);
            return signum ^ ((1 << 24) - 2 * FmExp2_Lookup(sx));
        }
    } else {
        const int shift = 26 - FM_TANH_LG_N_SAMPLES;
        int32_t lowbits = x & ((1 << shift) - 1);
        int32_t x_int = (x >> (shift - 1)) & ((FM_TANH_N_SAMPLES - 1) << 1);
        int32_t dy = fm_tanhtab[x_int];
        int32_t y0 = fm_tanhtab[x_int + 1];
        int32_t y = y0 + (int32_t)(((int64_t)dy * (int64_t)lowbits) >> shift);

        return y ^ signum;
    }
}
