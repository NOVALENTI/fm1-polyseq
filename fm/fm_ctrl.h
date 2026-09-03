/*
 * Copyright 2013 Google Inc. (adapted from Dexed/msfa controllers.h).
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

/* fm_ctrl.h — slimmed C99 port of the msfa Controllers state actually
 * consumed by the voice render path. Dropped vs upstream: MPE, SCL/KBM
 * tuning hooks, mod config-string parsing (UI), transpose switch, and
 * the FmCore back-pointer (render is driven by the voice manager).
 * applyMod uses integer math (cc * range / 100); upstream's float
 * 0.01 * range can differ by 1 LSB when modulation is active — inaudible
 * and documented; the all-zero default state is bit-identical. */

#ifndef FM_CTRL_H
#define FM_CTRL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_CTRL_PITCH 128
#define FM_CTRL_PITCH_RANGE_UP 129
#define FM_CTRL_PITCH_STEP 130
#define FM_CTRL_PITCH_RANGE_DN 131

typedef struct {
    int range;
    int pitch;
    int amp;
    int eg;
} FmCtlMod;

typedef struct {
    int values_[132];
    char opSwitch[7];
    int amp_mod;
    int pitch_mod;
    int eg_mod;
    int aftertouch_cc;
    int breath_cc;
    int foot_cc;
    int modwheel_cc;
    int portamento_enable_cc;
    int portamento_gliss_cc;
    int portamento_cc;
    int masterTune;
    FmCtlMod wheel;
    FmCtlMod foot;
    FmCtlMod breath;
    FmCtlMod at;
} FmCtrl;

void FmCtrl_Init(FmCtrl *c);
void FmCtrl_Refresh(FmCtrl *c);

#ifdef __cplusplus
}
#endif

#endif /* FM_CTRL_H */
