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

/* fm_porta.c — C99 port of Dexed/msfa Porta::init_sr. */

#include <math.h>

#include "fm_porta.h"
#include "fm_common.h"

int32_t fm_porta_rates[128];
int32_t fm_porta_rates_glissando[128];

void FmPorta_InitSr(double sample_rate)
{
    /* Boot-time only: double/pow ok here, never in render.
     * NOTE: step truncates to int32 BEFORE the float math (matches
     * upstream's const int32_t step); keeping the double division here
     * would mistune every portamento rate. */
    int i;
    const int32_t step = (1 << 24) / 12;
    for (i = 0; i < 128; ++i) {
        double sps = 2100.0 * pow(2.0, -0.062 * i);
        double spf = sps / sample_rate;
        double spp = spf * FM_N;
        fm_porta_rates[i] = (int32_t)(0.5 + step * spp);
        sps = 1300.0 * pow(2.0, -0.062 * i);
        spf = sps / sample_rate;
        spp = spf * FM_N;
        fm_porta_rates_glissando[i] = (int32_t)(0.5 + step * spp);
    }
}
