/*
 * Copyright 2017 Pascal Gauthier.
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

/* fm_env.c — C99 port of Dexed/msfa Env (ACCURATE_ENVELOPE path).
 * Statement-for-statement port; bool -> int. */

#include "fm_env.h"
#include "fm_common.h"

static uint32_t fm_env_sr_multiplier = (1u << 24);

static const int fm_env_levellut[] = {
    0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46
};

static const int fm_env_statics[] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250,
    882000, 705600, 705600, 584325, 507150, 502740, 441000, 418950,
    352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530,
    145530, 125685, 110250, 110250, 88200, 88200, 74970, 61740,
    61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562, 27562,
    22050, 18522, 17640, 15435, 14112, 13230, 11025, 9261, 9261, 7717,
    6615, 6615, 5512, 5512, 4410, 3969, 3969, 3439, 2866, 2690, 2249,
    1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705,
    573, 573, 529, 441, 441
    /* upstream: measured to R=76 only */
};

static void fm_env_advance(FmEnv *e, int newix);

void FmEnv_InitSr(double sample_rate)
{
    /* Boot-time only: libm ok here, never in render. */
    fm_env_sr_multiplier = (uint32_t)((44100.0 / sample_rate) * (1u << 24));
}

void FmEnv_Init(FmEnv *e, const int r[4], const int l[4], int ol,
                int rate_scaling)
{
    int i;
    e->initialised_ = 1;
    for (i = 0; i < 4; i++) {
        e->rates_[i] = r[i];
        e->levels_[i] = l[i];
    }
    e->outlevel_ = ol;
    e->rate_scaling_ = rate_scaling;
    e->level_ = 0;
    e->down_ = 1;
    fm_env_advance(e, 0);
}

int32_t FmEnv_GetSample(FmEnv *e)
{
    if (e->staticcount_) {
        e->staticcount_ -= FM_N;
        if (e->staticcount_ <= 0) {
            e->staticcount_ = 0;
            fm_env_advance(e, e->ix_ + 1);
        }
    }

    if (e->ix_ < 3 || ((e->ix_ < 4) && !e->down_)) {
        if (e->staticcount_) {
            ;
        } else if (e->rising_) {
            const int jumptarget = 1716;
            if (e->level_ < (jumptarget << 16)) {
                e->level_ = jumptarget << 16;
            }
            e->level_ += (((17 << 24) - e->level_) >> 24) * e->inc_;
            if (e->level_ >= e->targetlevel_) {
                e->level_ = e->targetlevel_;
                fm_env_advance(e, e->ix_ + 1);
            }
        } else {
            e->level_ -= e->inc_;
            if (e->level_ <= e->targetlevel_) {
                e->level_ = e->targetlevel_;
                fm_env_advance(e, e->ix_ + 1);
            }
        }
    }
    return e->level_;
}

void FmEnv_KeyDown(FmEnv *e, int d)
{
    if (e->down_ != d) {
        e->down_ = d;
        fm_env_advance(e, d ? 0 : 3);
    }
}

int FmEnv_ScaleOutLevel(int outlevel)
{
    return outlevel >= 20 ? 28 + outlevel : fm_env_levellut[outlevel];
}

static void fm_env_advance(FmEnv *e, int newix)
{
    e->ix_ = newix;
    if (e->ix_ < 4) {
        int newlevel = e->levels_[e->ix_];
        int actuallevel = FmEnv_ScaleOutLevel(newlevel) >> 1;
        int qrate;
        actuallevel = (actuallevel << 6) + e->outlevel_ - 4256;
        actuallevel = actuallevel < 16 ? 16 : actuallevel;
        e->targetlevel_ = actuallevel << 16;
        e->rising_ = (e->targetlevel_ > e->level_);

        qrate = (e->rates_[e->ix_] * 41) >> 6;
        qrate += e->rate_scaling_;
        qrate = fm_mini(qrate, 63);

        if (e->targetlevel_ == e->level_ || (e->ix_ == 0 && newlevel == 0)) {
            int staticrate = e->rates_[e->ix_];
            staticrate += e->rate_scaling_;
            staticrate = fm_mini(staticrate, 99);
            e->staticcount_ = staticrate < 77 ?
                fm_env_statics[staticrate] : 20 * (99 - staticrate);
            if (staticrate < 77 && (e->ix_ == 0 && newlevel == 0)) {
                e->staticcount_ /= 20;
            }
            e->staticcount_ = (int)(((int64_t)e->staticcount_ *
                                     (int64_t)fm_env_sr_multiplier) >> 24);
        } else {
            e->staticcount_ = 0;
        }
        e->inc_ = (4 + (qrate & 3)) << (2 + FM_LG_N + (qrate >> 2));
        e->inc_ = (int)(((int64_t)e->inc_ *
                         (int64_t)fm_env_sr_multiplier) >> 24);
    }
}

void FmEnv_Update(FmEnv *e, const int r[4], const int l[4], int ol,
                  int rate_scaling)
{
    int i;
    int newlevel;
    int actuallevel;
    for (i = 0; i < 4; i++) {
        e->rates_[i] = r[i];
        e->levels_[i] = l[i];
    }
    e->outlevel_ = ol;
    e->rate_scaling_ = rate_scaling;
    if (e->down_) {
        newlevel = e->levels_[2];
        actuallevel = FmEnv_ScaleOutLevel(newlevel) >> 1;
        actuallevel = (actuallevel << 6) - 4256;
        actuallevel = actuallevel < 16 ? 16 : actuallevel;
        e->targetlevel_ = actuallevel << 16;
        fm_env_advance(e, 2);
    }
}

void FmEnv_GetPosition(FmEnv *e, int8_t *step)
{
    *step = (int8_t)e->ix_;
}

void FmEnv_Transfer(FmEnv *e, const FmEnv *src)
{
    int i;
    for (i = 0; i < 4; i++) {
        e->rates_[i] = src->rates_[i];
        e->levels_[i] = src->levels_[i];
    }
    e->outlevel_ = src->outlevel_;
    e->rate_scaling_ = src->rate_scaling_;
    e->level_ = src->level_;
    e->targetlevel_ = src->targetlevel_;
    e->rising_ = src->rising_;
    e->ix_ = src->ix_;
    e->down_ = src->down_;
    e->staticcount_ = src->staticcount_;
    e->inc_ = src->inc_;
}

int FmEnv_IsActive(const FmEnv *e)
{
    return e->initialised_ && (e->ix_ < 4 || e->levels_[3] > 0);
}
