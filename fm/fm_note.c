/*
 * Copyright 2016-2025 Pascal Gauthier.
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

/* fm_note.c — C99 port of Dexed/msfa Dx7Note. See fm_note.h for the
 * documented divergences (tuning/MPE/float paths). */

#include <math.h>
#include <stdlib.h>

#include "fm_note.h"
#include "fm_common.h"
#include "fm_exp2.h"
#include "fm_freqlut.h"
#include "fm_porta.h"

#define FM_FEEDBACK_BITDEPTH 8

/* Standard 12-TET: (1<<24) * (log(440)/log(2) - 69/12), step (1<<24)/12. */
#define FM_STD_BASE 50857777
#define FM_STD_STEP ((1 << 24) / 12)

static int32_t fm_std_logfreq(int midinote)
{
    return FM_STD_BASE + FM_STD_STEP * midinote;
}

static const int32_t fm_coarsemul[] = {
    -16777216, 0, 16777216, 26591258, 33554432, 38955489, 43368474, 47099600,
    50331648, 53182516, 55732705, 58039632, 60145690, 62083076, 63876816,
    65546747, 67108864, 68576247, 69959732, 71268397, 72509921, 73690858,
    74816848, 75892776, 76922906, 77910978, 78860292, 79773775, 80654032,
    81503396, 82323963, 83117622
};

static int32_t fm_logfreq_round2semi(int freq)
{
    const int base = 50857777;
    const int step = (1 << 24) / 12;
    const int rem = (freq - base) % step;
    return freq - rem;
}

int32_t FmNote_OscFreq(int midinote, int mode, int coarse, int fine,
                       int detune, int channel)
{
    /* NOTE-ON time only: double/libm ok here, never in render.
     * channel is accepted for call parity (MTS paths dropped). */
    int32_t logfreq;
    (void)channel;
    if (mode == 0) {
        double detuneRatio;
        logfreq = fm_std_logfreq(midinote);
        /* Verbatim semantics (both quirks matter):
         *  - logfreq narrows through float32 in the exp argument;
         *  - the compound += converts the SUM (int32)(logfreq + term),
         *    NOT the term: casting the term first differs by 1 LSB
         *    whenever it is fractional. Do NOT "simplify" either. */
        detuneRatio = 0.0209 * exp(-0.396 * (((float)logfreq) / (1 << 24))) / 7;
        logfreq += detuneRatio * logfreq * (detune - 7);
        logfreq += fm_coarsemul[coarse & 31];
        if (fine) {
            logfreq += (int32_t)floor(24204406.323123 * log(1 + 0.01 * fine) + 0.5);
        }
    } else {
        logfreq = (4458616 * ((coarse & 3) * 100 + fine)) >> 3;
        logfreq += detune > 7 ? 13457 * (detune - 7) : 0;
    }
    return logfreq;
}

static const uint8_t fm_velocity_data[64] = {
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163,
    166, 170, 173, 174, 178, 181, 184, 186, 189, 190, 194, 196, 198, 200, 202,
    205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232,
    233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251,
    252, 253, 254
};

int FmNote_ScaleVelocity(int velocity, int sensitivity)
{
    int clamped_vel = fm_maxi(0, fm_mini(127, velocity));
    int vel_value = fm_velocity_data[clamped_vel >> 1] - 239;
    return ((sensitivity * vel_value + 7) >> 3) << 4;
}

int FmNote_ScaleRate(int midinote, int sensitivity)
{
    int x = fm_mini(31, fm_maxi(0, midinote / 3 - 7));
    return (sensitivity * x) >> 3;
}

static const uint8_t fm_exp_scale_data[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
    80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250
};

static int fm_scale_curve(int group, int depth, int curve)
{
    int scale;
    if (curve == 0 || curve == 3) {
        scale = (group * depth * 329) >> 12;
    } else {
        int n_scale_data = (int)sizeof(fm_exp_scale_data);
        int raw_exp = fm_exp_scale_data[fm_mini(group, n_scale_data - 1)];
        scale = (raw_exp * depth * 329) >> 15;
    }
    if (curve < 2) {
        scale = -scale;
    }
    return scale;
}

static int fm_scale_level(int midinote, int break_pt, int left_depth,
                          int right_depth, int left_curve, int right_curve)
{
    int offset = midinote - break_pt - 17;
    if (offset >= 0) {
        return fm_scale_curve((offset + 1) / 3, right_depth, right_curve);
    } else {
        return fm_scale_curve(-(offset - 1) / 3, left_depth, left_curve);
    }
}

static const uint8_t fm_pitchmodsenstab[] = {
    0, 10, 20, 33, 55, 92, 153, 255
};

static const uint32_t fm_ampmodsenstab[] = {
    0, 4342338, 7171437, 16777216
};

void FmNote_Init(FmNote *n, const uint8_t patch[156], int midinote,
                 int velocity, int channel, const FmCtrl *ctrls)
{
    int rates[4];
    int levels[4];
    int op;
    (void)ctrls;
    n->initialised_ = 1;
    n->playingMidiNote = (uint8_t)midinote;
    n->midiChannel = (uint8_t)channel;

    for (op = 0; op < 6; op++) {
        int off = op * 21;
        int i;
        int outlevel;
        int level_scaling;
        int mode;
        int coarse;
        int fine;
        int detune;
        int32_t freq;
        for (i = 0; i < 4; i++) {
            rates[i] = patch[off + i];
            levels[i] = patch[off + 4 + i];
        }
        outlevel = patch[off + 16];
        outlevel = FmEnv_ScaleOutLevel(outlevel);
        level_scaling = fm_scale_level(midinote, patch[off + 8], patch[off + 9],
                                       patch[off + 10], patch[off + 11],
                                       patch[off + 12]);
        outlevel += level_scaling;
        outlevel = fm_mini(127, outlevel);
        outlevel = outlevel << 5;
        outlevel += FmNote_ScaleVelocity(velocity, patch[off + 15]);
        outlevel = fm_maxi(0, outlevel);
        {
            int rate_scaling = FmNote_ScaleRate(midinote, patch[off + 13]);
            FmEnv_Init(&n->env_[op], rates, levels, outlevel, rate_scaling);
        }

        mode = patch[off + 17];
        coarse = patch[off + 18];
        fine = patch[off + 19];
        detune = patch[off + 20];
        freq = FmNote_OscFreq(midinote, mode, coarse, fine, detune, channel);
        n->opMode[op] = mode;
        n->basepitch_[op] = freq;
        n->porta_curpitch_[op] = freq;
        n->ampmodsens_[op] = (int32_t)fm_ampmodsenstab[patch[off + 14] & 3];
        n->params_[op].phase = 0;
        n->params_[op].gain_out = 0;
    }
    for (op = 0; op < 4; op++) {
        rates[op] = patch[126 + op];
        levels[op] = patch[130 + op];
    }
    FmPitchEnv_Set(&n->pitchenv_, rates, levels);
    n->algorithm_ = patch[134];
    {
        int feedback = patch[135];
        n->fb_shift_ = feedback != 0 ? FM_FEEDBACK_BITDEPTH - feedback : 16;
    }
    n->pitchmoddepth_ = (patch[139] * 165) >> 6;
    n->pitchmodsens_ = fm_pitchmodsenstab[patch[143] & 7];
    n->ampmoddepth_ = (patch[140] * 165) >> 6;
    /* Defined startup: upstream leaves fb_buf_ uninitialized and the
     * feedback operator reads it on the first block (stock Dexed output
     * there is stack-garbage nondeterministic, converging in ~2 samples).
     * Zeroed here for deterministic firmware behavior. */
    n->fb_buf_[0] = 0;
    n->fb_buf_[1] = 0;
}

void FmNote_InitPortamento(FmNote *n, const FmNote *src)
{
    int i;
    for (i = 0; i < 6; i++) {
        n->porta_curpitch_[i] = src->porta_curpitch_[i];
    }
}

void FmNote_Compute(FmNote *n, FmCoreState *cs, int32_t *buf,
                    int32_t lfo_val, int32_t lfo_delay, const FmCtrl *ctrls)
{
    /* ==== PITCH ==== */
    uint32_t pmd = (uint32_t)n->pitchmoddepth_ * (uint32_t)lfo_delay;
    int32_t senslfo = n->pitchmodsens_ * (lfo_val - (1 << 23));
    int32_t pmod_1 = (int32_t)(((int64_t)pmd) * (int64_t)senslfo >> 39);
    int32_t pmod_2;
    int32_t pitch_mod;
    int pitchbend;
    int32_t pb;
    int32_t pitch_base;
    uint32_t amod_1;
    uint32_t amod_2;
    uint32_t amd_mod;
    uint32_t amod_3;
    int porta_rate;
    int op;
    pmod_1 = pmod_1 < 0 ? -pmod_1 : pmod_1;
    pmod_2 = (int32_t)(((int64_t)ctrls->pitch_mod * (int64_t)senslfo) >> 14);
    pmod_2 = pmod_2 < 0 ? -pmod_2 : pmod_2;
    pitch_mod = fm_maxi(pmod_1, pmod_2);
    pitch_mod = FmPitchEnv_GetSample(&n->pitchenv_) +
                (pitch_mod * (senslfo < 0 ? -1 : 1));

    /* ---- PITCH BEND (integer; identical at center detent) ---- */
    pitchbend = ctrls->values_[FM_CTRL_PITCH];
    pb = (pitchbend - 0x2000);
    if (pb != 0) {
        if (ctrls->values_[FM_CTRL_PITCH_STEP] == 0) {
            int32_t range = (pb >= 0) ?
                ctrls->values_[FM_CTRL_PITCH_RANGE_UP] :
                ctrls->values_[FM_CTRL_PITCH_RANGE_DN];
            pb = (int32_t)(((int64_t)(pb << 11) * range) / 12);
        } else {
            int stp = 12 / ctrls->values_[FM_CTRL_PITCH_STEP];
            pb = pb * stp / 8191;
            pb = (pb * (8191 / stp)) << 11;
        }
    }
    /* MPE + scale-tuning branches dropped (dead without MPE/scale input). */

    pitch_base = pb + ctrls->masterTune;
    pitch_mod += pitch_base;

    /* ==== AMP MOD ==== */
    lfo_val = (1 << 24) - lfo_val;
    amod_1 = (uint32_t)(((int64_t)n->ampmoddepth_ * (int64_t)lfo_delay) >> 8);
    amod_1 = (uint32_t)(((int64_t)amod_1 * (int64_t)lfo_val) >> 24);
    amod_2 = (uint32_t)(((int64_t)ctrls->amp_mod * (int64_t)lfo_val) >> 7);
    amd_mod = amod_1 > amod_2 ? amod_1 : amod_2;

    /* ==== EG AMP MOD ==== */
    amod_3 = (uint32_t)((ctrls->eg_mod + 1) << 17);
    {
        uint32_t floor_amd = (uint32_t)(1 << 24) - amod_3;
        amd_mod = floor_amd > amd_mod ? floor_amd : amd_mod;
    }

    if (ctrls->portamento_enable_cc) {
        if (ctrls->portamento_gliss_cc) {
            porta_rate = fm_porta_rates_glissando[ctrls->portamento_cc];
        } else {
            porta_rate = fm_porta_rates[ctrls->portamento_cc];
        }
    } else {
        porta_rate = fm_porta_rates[0];
    }

    /* ==== OP RENDER ==== */
    for (op = 0; op < 6; op++) {
        if (ctrls->opSwitch[op] == '0') {
            FmEnv_GetSample(&n->env_[op]);
            n->params_[op].level_in = 0;
        } else {
            int32_t basepitch = n->basepitch_[op];
            if (n->opMode[op]) {
                n->params_[op].freq =
                    FmFreqlut_Lookup(basepitch + pitch_base);
            } else {
                if (n->porta_curpitch_[op] != n->basepitch_[op]) {
                    int32_t cur;
                    int32_t dst;
                    int going_up;
                    int32_t newpitch;
                    basepitch = n->porta_curpitch_[op];
                    if (ctrls->portamento_gliss_cc) {
                        basepitch = fm_logfreq_round2semi(basepitch);
                    }
                    cur = n->porta_curpitch_[op];
                    dst = n->basepitch_[op];
                    going_up = cur < dst;
                    newpitch = cur + (going_up ? +porta_rate : -porta_rate);
                    if ((going_up && newpitch > dst) ||
                        (!going_up && newpitch < dst)) {
                        newpitch = dst;
                    }
                    n->porta_curpitch_[op] = newpitch;
                }
                n->params_[op].freq =
                    FmFreqlut_Lookup(basepitch + pitch_mod);
            }

            {
                int32_t level = FmEnv_GetSample(&n->env_[op]);
                if (n->ampmodsens_[op] != 0) {
                    uint32_t sensamp =
                        (uint32_t)(((uint64_t)amd_mod *
                                    (uint64_t)(uint32_t)n->ampmodsens_[op]) >> 24);
                    uint32_t pt = FmNote_AmpCurve(sensamp);
                    /* NOTE: (uint64_t)level sign-extends intentional:
                     * upstream writes it exactly so, and ducked (negative)
                     * levels flow through the unsigned multiply mod 2^64.
                     * Zero-extending here diverges once level goes
                     * negative under strong amp modulation. */
                    uint32_t ldiff =
                        (uint32_t)(((uint64_t)level *
                                    ((uint64_t)pt << 4)) >> 28);
                    /* Unsigned subtraction EXACTLY as upstream
                     * (`level -= ldiff` with ldiff uint32_t promotes level
                     * to uint32 and wraps mod 2^32). A signed
                     * `level -= (int32_t)ldiff` looks equivalent but is
                     * SIGNED-overflow UB when ducking drives level
                     * negative — the optimizer WILL exploit it. */
                    level = (int32_t)((uint32_t)level - ldiff);
                }
                n->params_[op].level_in = level;
            }
        }
    }
    FmCore_Render(cs, buf, n->params_, n->algorithm_, n->fb_buf_,
                  n->fb_shift_);
}

void FmNote_KeyUp(FmNote *n)
{
    int op;
    for (op = 0; op < 6; op++) {
        FmEnv_KeyDown(&n->env_[op], 0);
    }
    FmPitchEnv_KeyDown(&n->pitchenv_, 0);
}

void FmNote_Update(FmNote *n, const uint8_t patch[156], int midinote,
                   int velocity, int channel)
{
    int rates[4];
    int levels[4];
    int op;
    n->playingMidiNote = (uint8_t)midinote;
    n->midiChannel = (uint8_t)channel;

    for (op = 0; op < 6; op++) {
        int off = op * 21;
        int mode = patch[off + 17];
        int coarse = patch[off + 18];
        int fine = patch[off + 19];
        int detune = patch[off + 20];
        int i;
        int outlevel;
        int level_scaling;
        int rate_scaling;
        n->basepitch_[op] =
            FmNote_OscFreq(midinote, mode, coarse, fine, detune, channel);
        n->ampmodsens_[op] = (int32_t)fm_ampmodsenstab[patch[off + 14] & 3];
        n->opMode[op] = mode;

        for (i = 0; i < 4; i++) {
            rates[i] = patch[off + i];
            levels[i] = patch[off + 4 + i];
        }
        outlevel = patch[off + 16];
        outlevel = FmEnv_ScaleOutLevel(outlevel);
        level_scaling = fm_scale_level(midinote, patch[off + 8], patch[off + 9],
                                       patch[off + 10], patch[off + 11],
                                       patch[off + 12]);
        outlevel += level_scaling;
        outlevel = fm_mini(127, outlevel);
        outlevel = outlevel << 5;
        outlevel += FmNote_ScaleVelocity(velocity, patch[off + 15]);
        outlevel = fm_maxi(0, outlevel);
        rate_scaling = FmNote_ScaleRate(midinote, patch[off + 13]);
        FmEnv_Update(&n->env_[op], rates, levels, outlevel, rate_scaling);
    }
    n->algorithm_ = patch[134];
    {
        int feedback = patch[135];
        n->fb_shift_ = feedback != 0 ? FM_FEEDBACK_BITDEPTH - feedback : 16;
    }
    n->pitchmoddepth_ = (patch[139] * 165) >> 6;
    n->pitchmodsens_ = fm_pitchmodsenstab[patch[143] & 7];
    n->ampmoddepth_ = (patch[140] * 165) >> 6;
}

void FmNote_PeekStatus(FmNote *n, FmVoiceStatus *status)
{
    int i;
    for (i = 0; i < 6; i++) {
        status->amp[i] =
            (uint32_t)FmExp2_Lookup(n->params_[i].level_in - (14 * (1 << 24)));
        FmEnv_GetPosition(&n->env_[i], &status->ampStep[i]);
    }
    FmPitchEnv_GetPosition(&n->pitchenv_, &status->pitchStep);
}

void FmNote_TransferState(FmNote *n, const FmNote *src)
{
    int i;
    for (i = 0; i < 6; i++) {
        FmEnv_Transfer(&n->env_[i], &src->env_[i]);
        n->params_[i].gain_out = src->params_[i].gain_out;
        n->params_[i].phase = src->params_[i].phase;
    }
}

void FmNote_TransferSignal(FmNote *n, const FmNote *src)
{
    int i;
    for (i = 0; i < 6; i++) {
        n->params_[i].gain_out = src->params_[i].gain_out;
        n->params_[i].phase = src->params_[i].phase;
    }
}

void FmNote_TransferPhase(FmNote *n, const FmNote *src)
{
    int i;
    for (i = 0; i < 6; i++) {
        n->params_[i].phase = src->params_[i].phase;
    }
}

void FmNote_OscSync(FmNote *n)
{
    int i;
    for (i = 0; i < 6; i++) {
        n->params_[i].gain_out = 0;
        n->params_[i].phase = 0;
    }
}

int FmNote_IsPlaying(FmNote *n)
{
    int i;
    if (!n->initialised_) {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        if (FmCore_IsCarrier(n->algorithm_, i) &&
            FmEnv_IsActive(&n->env_[i])) {
            return 1;
        }
    }
    return 0;
}
