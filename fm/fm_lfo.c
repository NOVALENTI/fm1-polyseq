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

/* fm_lfo.c — C99 port of Dexed/msfa Lfo. bool -> int. */

#include "fm_lfo.h"
#include "fm_common.h"
#include "fm_sin.h"

static const double fm_lfo_source[] = {
    0.062541, 0.125031, 0.312393, 0.437120, 0.624610,
    0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
    1.560915, 1.752081, 1.875117, 2.062494, 2.247191,
    2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
    3.188013, 3.369840, 3.500175, 3.682224, 3.812065,
    4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
    4.814636, 4.930480, 5.121901, 5.315191, 5.434783,
    5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
    6.431695, 6.564264, 6.749460, 6.868132, 7.052186,
    7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
    7.993605, 8.181967, 8.372405, 8.504848, 8.685079,
    8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
    9.607994, 9.798158, 9.950249, 10.117361, 11.251125,
    11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
    16.366612, 16.638935, 17.869907, 19.193858, 19.425019,
    20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
    25.746653, 27.173913, 27.578599, 29.052876, 30.693677,
    31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
    38.197097, 38.550501, 40.387722, 40.749796, 42.625746,
    44.326241, 44.883303, 46.772685, 48.590865, 49.261084
};

static uint32_t fm_lfo_unit_;
static uint32_t fm_lfo_ratio_;

void FmLfo_Init(double sample_rate)
{
    /* Boot-time only: double ok here, never in render. */
    fm_lfo_unit_ = (uint32_t)(FM_N * 25190424 / sample_rate + 0.5);
    fm_lfo_ratio_ = (uint32_t)(4437500000.0 * FM_N / sample_rate);
}

void FmLfo_Reset(FmLfo *l, const uint8_t params[6])
{
    int rate = params[0];
    int a;
    /* Defined-startup deviation from upstream (see header): zero the
     * phase and S&H state that upstream leaves uninitialized. */
    l->phase_ = 0u;
    l->randstate_ = 0u;
    l->delaystate_ = 0u;
    l->delta_ = (uint32_t)(fm_lfo_source[rate] * fm_lfo_ratio_);
    a = 99 - params[1];
    if (a == 99) {
        l->delayinc_ = ~0u;
        l->delayinc2_ = ~0u;
    } else {
        a = (16 + (a & 15)) << (1 + (a >> 4));
        l->delayinc_ = fm_lfo_unit_ * (uint32_t)a;
        a &= 0xff80;
        a = fm_maxi(0x80, a);
        l->delayinc2_ = fm_lfo_unit_ * (uint32_t)a;
    }
    l->waveform_ = params[5];
    l->sync_ = params[4] != 0;
}

int32_t FmLfo_GetSample(FmLfo *l)
{
    int32_t x;
    l->phase_ += l->delta_;
    switch (l->waveform_) {
    case 0: /* triangle */
        x = (int32_t)(l->phase_ >> 7);
        x ^= -(int32_t)(l->phase_ >> 31);
        x &= (1 << 24) - 1;
        return x;
    case 1: /* sawtooth down */
        return (int32_t)((~l->phase_ ^ (1U << 31)) >> 8);
    case 2: /* sawtooth up */
        return (int32_t)((l->phase_ ^ (1U << 31)) >> 8);
    case 3: /* square */
        return (int32_t)(((~l->phase_) >> 7) & (1 << 24));
    case 4: /* sine */
        return (1 << 23) + (FmSin_Lookup((int32_t)(l->phase_ >> 8)) >> 1);
    case 5: /* sample & hold */
        if (l->phase_ < l->delta_) {
            l->randstate_ = (uint8_t)((l->randstate_ * 179 + 17) & 0xff);
        }
        x = l->randstate_ ^ 0x80;
        return (x + 1) << 16;
    default:
        break;
    }
    return 1 << 23;
}

int32_t FmLfo_GetDelay(FmLfo *l)
{
    uint32_t delta = l->delaystate_ < (1U << 31) ? l->delayinc_ : l->delayinc2_;
    uint64_t d = (uint64_t)l->delaystate_ + delta;
    if (d > ~0u) {
        return 1 << 24;
    }
    l->delaystate_ = (uint32_t)d;
    if (d < (1U << 31)) {
        return 0;
    } else {
        return (int32_t)((d >> 7) & ((1 << 24) - 1));
    }
}

void FmLfo_KeyDown(FmLfo *l)
{
    if (l->sync_) {
        l->phase_ = (1U << 31) - 1;
    }
    l->delaystate_ = 0;
}
