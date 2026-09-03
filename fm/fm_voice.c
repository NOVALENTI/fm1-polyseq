/*
 * Copyright 2026 FM-1 project (voice manager over ported msfa DSP core).
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

/* fm_voice.c — implements fm_stub.h: 12 static voices, caller-allocated.
 * Render mirrors Dexed's EngineMkI block loop (PluginProcessor.cpp):
 * per 64-sample sub-block, step the single global LFO once, then render
 * each sounding voice additively into an int32 mix buffer; output scaling
 * mirrors upstream exactly (val>>4, clip to +-2^24, >>9, /32768.0f).
 * Voices auto-free when their carriers go silent (release tails included).
 * No heap, no per-render allocation, bounded work. */

#include "fm_stub.h"
#include "fm_voice.h"
#include "fm_note.h"
#include "fm_lfo.h"
#include "fm_ctrl.h"
#include "fm_core.h"
#include "fm_sin.h"
#include "fm_exp2.h"
#include "fm_freqlut.h"
#include "fm_env.h"
#include "fm_pitchenv.h"
#include "fm_porta.h"

/* Default INIT voice (156-byte DX7 layout): algorithm 32 (index 31, all
 * carriers), ops 0..4 silent, op 5 a clean sine-ish pluck (medium
 * attack/decay, full sustain, clean release), LFO parked (PMD/AMD 0),
 * flat pitch envelope. Audible on any NoteOn out of reset. */
static const uint8_t fm_init_voice[156] = {
    /* op0..op4: silent (OUT 0), sane ratio unison params */
    50, 50, 50, 50, 0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 7,
    50, 50, 50, 50, 0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 7,
    50, 50, 50, 50, 0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 7,
    50, 50, 50, 50, 0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 7,
    50, 50, 50, 50, 0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 7,
    /* op5: rates 95/70/50/40, levels 99/85/85/0, vel-sens 4, OUT 99,
     * ratio x1, detune center */
    95, 70, 50, 40, 99, 85, 85, 0, 60, 0, 0, 0, 0, 0, 4, 0, 99, 0, 1,
    0, 7,
    /* pitch EG: fast rates, flat levels (tab[50] == 0 -> static) */
    99, 99, 99, 50, 50, 50, 50, 50,
    /* algorithm 32 (0-based 31), feedback off, osc sync off */
    31, 0, 0,
    /* LFO: speed 35, delay 0, PMD 0, AMD 0, sync 1, triangle */
    35, 0, 0, 0, 1, 0,
    /* PMS 0, remaining bytes (name area upstream) zero */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static uint8_t fm_patch[156];
static uint8_t fm_lfo_params[6];

static FmNote fm_voices[FM_NUM_VOICES];
static uint8_t fm_active[FM_NUM_VOICES];
static uint8_t fm_midi[FM_NUM_VOICES];

static FmCoreState fm_core_state;
static FmCtrl fm_ctrls;
static FmLfo fm_lfo;

static int32_t fm_mix[64];

void FM_LoadPatch(const uint8_t patch[156])
{
    int i;
    if (patch == 0) {
        return;
    }
    for (i = 0; i < 156; i++) {
        fm_patch[i] = patch[i];
    }
    for (i = 0; i < 6; i++) {
        fm_lfo_params[i] = patch[137 + i];
    }
    FmLfo_Reset(&fm_lfo, fm_lfo_params);
}

void FM_Init(uint32_t sample_rate)
{
    double sr = (double)sample_rate;
    uint8_t i;
    FmSin_Init();
    FmExp2_Init();
    FmTanh_Init();
    FmFreqlut_Init(sr);
    FmEnv_InitSr(sr);
    FmPitchEnv_InitSr(sr);
    FmLfo_Init(sr);
    FmPorta_InitSr(sr);
    FmCtrl_Init(&fm_ctrls);
    fm_ctrls.values_[FM_CTRL_PITCH] = 0x2000;
    FmCtrl_Refresh(&fm_ctrls);
    for (i = 0; i < FM_NUM_VOICES; i++) {
        fm_active[i] = 0;
        fm_midi[i] = 0xFF;
    }
    FM_LoadPatch(fm_init_voice);
}

void FM_NoteOn(uint8_t voice_id, uint8_t midi_note, uint8_t velocity)
{
    if (voice_id >= FM_NUM_VOICES || midi_note > 127) {
        return;
    }
    if (velocity > 127u) {
        velocity = 127;
    }
    FmNote_Init(&fm_voices[voice_id], fm_patch, midi_note, velocity, 1,
                &fm_ctrls);
    fm_active[voice_id] = 1;
    fm_midi[voice_id] = midi_note;
    FmLfo_KeyDown(&fm_lfo);
}

void FM_NoteOff(uint8_t voice_id)
{
    if (voice_id >= FM_NUM_VOICES) {
        return;
    }
    FmNote_KeyUp(&fm_voices[voice_id]);
}

/* Upstream output scaling, verbatim semantics (PluginProcessor.cpp):
 * val>>4, hard clip to +-2^24, >>9 to s16 range, /32768.0f. Dual-mono. */
static float fm_scale_out(int32_t val)
{
    int32_t clip_val;
    val = val >> 4;
    if (val < -(1 << 24)) {
        clip_val = 0x8000;
    } else if (val >= (1 << 24)) {
        clip_val = 0x7fff;
    } else {
        clip_val = val >> 9;
    }
    return ((float)clip_val) / (float)0x8000;
}

void FM_Render(float *left_out, float *right_out, uint16_t num_frames)
{
    uint16_t done = 0;
    uint8_t v;
    int i;
    if (left_out == 0 || right_out == 0 || num_frames == 0u) {
        return;
    }
    while (done < num_frames) {
        uint16_t n = num_frames - done;
        int32_t lfo_val;
        int32_t lfo_dly;
        if (n > 64) {
            n = 64;
        }
        for (i = 0; i < 64; i++) {
            fm_mix[i] = 0;
        }
        lfo_val = FmLfo_GetSample(&fm_lfo);
        lfo_dly = FmLfo_GetDelay(&fm_lfo);
        for (v = 0; v < FM_NUM_VOICES; v++) {
            if (fm_active[v]) {
                FmNote_Compute(&fm_voices[v], &fm_core_state, fm_mix,
                               lfo_val, lfo_dly, &fm_ctrls);
            }
        }
        for (i = 0; i < (int)n; i++) {
            float s = fm_scale_out(fm_mix[i]);
            left_out[done + i] = s;
            right_out[done + i] = s;
        }
        done += n;
    }
    /* Auto-free finished voices (release tails included). */
    for (v = 0; v < FM_NUM_VOICES; v++) {
        if (fm_active[v] && !FmNote_IsPlaying(&fm_voices[v])) {
            fm_active[v] = 0;
            fm_midi[v] = 0xFF;
        }
    }
}
