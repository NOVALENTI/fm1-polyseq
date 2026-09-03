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

/* fm_op_kernel.c — C99 port of the scalar FmOpKernel loops.
 * Statement-for-statement port (bool -> int). Inner loop is one sine
 * lookup + one 32x32->64 MAC per sample: the pi32v2 Type-III dual-MAC
 * inline-asm target (see audio_core.c Region B). */

#include "fm_op_kernel.h"
#include "fm_common.h"
#include "fm_sin.h"

/* NOTE: upstream keeps a file-scope zero bus buffer for the NEON path;
 * omitted here (scalar path only); dx7note reintroduces its own. */

void FmOpKernel_Compute(int32_t *output, const int32_t *input,
                        int32_t phase0, int32_t freq,
                        int32_t gain1, int32_t gain2, int add)
{
    int32_t dgain = (gain2 - gain1 + (FM_N >> 1)) >> FM_LG_N;
    int32_t gain = gain1;
    int32_t phase = phase0;
    int i;
    if (add) {
        for (i = 0; i < FM_N; i++) {
            int32_t y;
            int32_t y1;
            gain += dgain;
            y = FmSin_Lookup(phase + input[i]);
            y1 = (int32_t)(((int64_t)y * (int64_t)gain) >> 24);
            output[i] += y1;
            phase += freq;
        }
    } else {
        for (i = 0; i < FM_N; i++) {
            int32_t y;
            int32_t y1;
            gain += dgain;
            y = FmSin_Lookup(phase + input[i]);
            y1 = (int32_t)(((int64_t)y * (int64_t)gain) >> 24);
            output[i] = y1;
            phase += freq;
        }
    }
}

void FmOpKernel_ComputePure(int32_t *output, int32_t phase0, int32_t freq,
                            int32_t gain1, int32_t gain2, int add)
{
    int32_t dgain = (gain2 - gain1 + (FM_N >> 1)) >> FM_LG_N;
    int32_t gain = gain1;
    int32_t phase = phase0;
    int i;
    if (add) {
        for (i = 0; i < FM_N; i++) {
            int32_t y;
            int32_t y1;
            gain += dgain;
            y = FmSin_Lookup(phase);
            y1 = (int32_t)(((int64_t)y * (int64_t)gain) >> 24);
            output[i] += y1;
            phase += freq;
        }
    } else {
        for (i = 0; i < FM_N; i++) {
            int32_t y;
            int32_t y1;
            gain += dgain;
            y = FmSin_Lookup(phase);
            y1 = (int32_t)(((int64_t)y * (int64_t)gain) >> 24);
            output[i] = y1;
            phase += freq;
        }
    }
}

void FmOpKernel_ComputeFb(int32_t *output, int32_t phase0, int32_t freq,
                          int32_t gain1, int32_t gain2,
                          int32_t *fb_buf, int fb_shift, int add)
{
    int32_t dgain = (gain2 - gain1 + (FM_N >> 1)) >> FM_LG_N;
    int32_t gain = gain1;
    int32_t phase = phase0;
    int32_t y0 = fb_buf[0];
    int32_t y = fb_buf[1];
    int i;
    if (add) {
        for (i = 0; i < FM_N; i++) {
            int32_t scaled_fb;
            gain += dgain;
            scaled_fb = (y0 + y) >> (fb_shift + 1);
            y0 = y;
            y = FmSin_Lookup(phase + scaled_fb);
            y = (int32_t)(((int64_t)y * (int64_t)gain) >> 24);
            output[i] += y;
            phase += freq;
        }
    } else {
        for (i = 0; i < FM_N; i++) {
            int32_t scaled_fb;
            gain += dgain;
            scaled_fb = (y0 + y) >> (fb_shift + 1);
            y0 = y;
            y = FmSin_Lookup(phase + scaled_fb);
            y = (int32_t)(((int64_t)y * (int64_t)gain) >> 24);
            output[i] = y;
            phase += freq;
        }
    }
    fb_buf[0] = y0;
    fb_buf[1] = y;
}
