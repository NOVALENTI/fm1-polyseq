/*
 * Copyright 2013 Google Inc. (adapted from Dexed/msfa controllers).
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

/* fm_ctrl.c — C99 port of msfa Controllers refresh path. */

#include "fm_ctrl.h"
#include "fm_common.h"

static void fm_ctrl_apply_mod(FmCtrl *c, int cc, const FmCtlMod *mod)
{
    /* Upstream: (int)(cc * (0.01f * range)). Integer form differs by at
     * most 1 LSB (see header); control-rate only, never render. */
    int total = (cc * mod->range) / 100;
    if (mod->amp) {
        c->amp_mod = fm_maxi(c->amp_mod, total);
    }
    if (mod->pitch) {
        c->pitch_mod = fm_maxi(c->pitch_mod, total);
    }
    if (mod->eg) {
        c->eg_mod = fm_maxi(c->eg_mod, total);
    }
}

void FmCtrl_Init(FmCtrl *c)
{
    int i;
    for (i = 0; i < 132; i++) {
        c->values_[i] = 0;
    }
    c->amp_mod = 0;
    c->pitch_mod = 0;
    c->eg_mod = 0;
    /* All operators on; explicit loop keeps libc strcpy out. */
    for (i = 0; i < 6; i++) {
        c->opSwitch[i] = '1';
    }
    c->opSwitch[6] = '\0';
    c->aftertouch_cc = 0;
    c->breath_cc = 0;
    c->foot_cc = 0;
    c->modwheel_cc = 0;
    c->portamento_enable_cc = 0;
    c->portamento_gliss_cc = 0;
    c->portamento_cc = 0;
    c->masterTune = 0;
    c->wheel.range = 0;
    c->wheel.pitch = 0;
    c->wheel.amp = 0;
    c->wheel.eg = 0;
    c->foot.range = 0;
    c->foot.pitch = 0;
    c->foot.amp = 0;
    c->foot.eg = 0;
    c->breath.range = 0;
    c->breath.pitch = 0;
    c->breath.amp = 0;
    c->breath.eg = 0;
    c->at.range = 0;
    c->at.pitch = 0;
    c->at.amp = 0;
    c->at.eg = 0;
}

void FmCtrl_Refresh(FmCtrl *c)
{
    c->amp_mod = 0;
    c->pitch_mod = 0;
    c->eg_mod = 0;

    fm_ctrl_apply_mod(c, c->modwheel_cc, &c->wheel);
    fm_ctrl_apply_mod(c, c->breath_cc, &c->breath);
    fm_ctrl_apply_mod(c, c->foot_cc, &c->foot);
    fm_ctrl_apply_mod(c, c->aftertouch_cc, &c->at);

    if (!((c->wheel.eg || c->foot.eg) || (c->breath.eg || c->at.eg))) {
        c->eg_mod = 127;
    }
}
