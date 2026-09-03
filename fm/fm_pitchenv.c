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

/* fm_pitchenv.c — C99 port of Dexed/msfa PitchEnv. bool -> int. */

#include "fm_pitchenv.h"
#include "fm_common.h"

static int fm_pitchenv_unit_;

static const uint8_t fm_pitchenv_rate[] = {
    1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
    12, 13, 13, 14, 14, 15, 16, 16, 17, 18, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47,
    49, 51, 53, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82,
    85, 88, 91, 94, 98, 102, 106, 110, 115, 120, 125, 130, 135, 141, 147,
    153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255
};

static const int8_t fm_pitchenv_tab[] = {
    -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43,
    -41, -39, -37, -35, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24,
    -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,
    -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73,
    82, 92, 103, 115, 127
};

static void fm_pitchenv_advance(FmPitchEnv *e, int newix);

void FmPitchEnv_InitSr(double sample_rate)
{
    /* Boot-time only: double ok here, never in render. */
    fm_pitchenv_unit_ = (int)(FM_N * (1 << 24) / (21.3 * sample_rate) + 0.5);
}

void FmPitchEnv_Set(FmPitchEnv *e, const int r[4], const int l[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        e->rates_[i] = r[i];
        e->levels_[i] = l[i];
    }
    e->level_ = (int32_t)fm_pitchenv_tab[l[3]] << 19;
    e->down_ = 1;
    fm_pitchenv_advance(e, 0);
}

int32_t FmPitchEnv_GetSample(FmPitchEnv *e)
{
    if (e->ix_ < 3 || ((e->ix_ < 4) && !e->down_)) {
        if (e->rising_) {
            e->level_ += e->inc_;
            if (e->level_ >= e->targetlevel_) {
                e->level_ = e->targetlevel_;
                fm_pitchenv_advance(e, e->ix_ + 1);
            }
        } else {
            e->level_ -= e->inc_;
            if (e->level_ <= e->targetlevel_) {
                e->level_ = e->targetlevel_;
                fm_pitchenv_advance(e, e->ix_ + 1);
            }
        }
    }
    return e->level_;
}

void FmPitchEnv_KeyDown(FmPitchEnv *e, int d)
{
    if (e->down_ != d) {
        e->down_ = d;
        fm_pitchenv_advance(e, d ? 0 : 3);
    }
}

static void fm_pitchenv_advance(FmPitchEnv *e, int newix)
{
    e->ix_ = newix;
    if (e->ix_ < 4) {
        int newlevel = e->levels_[e->ix_];
        e->targetlevel_ = (int)fm_pitchenv_tab[newlevel] << 19;
        e->rising_ = (e->targetlevel_ > e->level_);
        e->inc_ = fm_pitchenv_rate[e->rates_[e->ix_]] * fm_pitchenv_unit_;
    }
}

void FmPitchEnv_GetPosition(FmPitchEnv *e, int8_t *step)
{
    *step = (int8_t)e->ix_;
}
