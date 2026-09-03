/*
 * Copyright 2016-2017 Pascal Gauthier.
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

/* fm_note.h — C99 port of Dexed/msfa Dx7Note (dx7note.h/dx7note.cc).
 *
 * Deliberate, documented divergences from upstream (all inaudible-or-better
 * in the default state; the bit-exact test pins the default state):
 *  - Standard 12-TET tuning only (logfreq table inline). No SCL/KBM, no
 *    MTS-ESP, no MPE branches: with no MPE/scale input the dropped code
 *    is provably dead (mpePitchBend 8192 -> d == 0; standard -> skip).
 *  - Pitch bend uses integer math ((pb<<11)*range/12 via int64) instead
 *    of float; identical at center detent (path skipped when pb == 0),
 *    sub-cent differences only while bending.
 *  - Amp-mod-sens curve uses an Exp2-table integer approximation of
 *    upstream's double exp() hack (see fm_note.c); <1% relative error,
 *    no soft-float in render, deterministic across libms.
 *  - Controllers reduced to FmCtrl (see fm_ctrl.h).
 *  - VoiceStatus steps are int8_t (upstream char is impl-defined signed).
 *  - Dropped: updateBasePitches (MTS-only), currentPatch/MTS/MPE fields.
 * Render adds into buf (caller zeroes once, then one compute per voice). */

#ifndef FM_NOTE_H
#define FM_NOTE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "fm_env.h"
#include "fm_pitchenv.h"
#include "fm_core.h"
#include "fm_ctrl.h"
#include "fm_curve.h"

typedef struct {
    uint32_t amp[6];
    int8_t ampStep[6];
    int8_t pitchStep;
} FmVoiceStatus;

typedef struct {
    int initialised_;
    FmEnv env_[6];
    FmOp params_[6];
    FmPitchEnv pitchenv_;
    int32_t basepitch_[6];
    int32_t fb_buf_[2];
    int32_t fb_shift_;
    int32_t ampmodsens_[6];
    int opMode[6];
    uint8_t playingMidiNote;
    uint8_t midiChannel;
    int ampmoddepth_;
    int algorithm_;
    int pitchmoddepth_;
    int pitchmodsens_;
    int32_t porta_curpitch_[6];
} FmNote;

/* patch: 156-byte DX7 voice dump. */
void FmNote_Init(FmNote *n, const uint8_t patch[156], int midinote,
                 int velocity, int channel, const FmCtrl *ctrls);
void FmNote_InitPortamento(FmNote *n, const FmNote *src);
void FmNote_Compute(FmNote *n, FmCoreState *cs, int32_t *buf,
                    int32_t lfo_val, int32_t lfo_delay, const FmCtrl *ctrls);
void FmNote_KeyUp(FmNote *n);
int FmNote_IsPlaying(FmNote *n);
void FmNote_Update(FmNote *n, const uint8_t patch[156], int midinote,
                   int velocity, int channel);
void FmNote_PeekStatus(FmNote *n, FmVoiceStatus *status);
void FmNote_TransferState(FmNote *n, const FmNote *src);
void FmNote_TransferSignal(FmNote *n, const FmNote *src);
void FmNote_TransferPhase(FmNote *n, const FmNote *src);
void FmNote_OscSync(FmNote *n);

/* Helpers (also used by the voice manager). */
int32_t FmNote_OscFreq(int midinote, int mode, int coarse, int fine,
                       int detune, int channel);
int FmNote_ScaleVelocity(int velocity, int sensitivity);
int FmNote_ScaleRate(int midinote, int sensitivity);

#ifdef __cplusplus
}
#endif

#endif /* FM_NOTE_H */
