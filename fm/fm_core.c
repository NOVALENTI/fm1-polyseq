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

/* fm_core.c — C99 port of Dexed/msfa FmCore::render/isCarrier/n_out.
 * Statement-for-statement port (bool -> int). Requires FM_N == 64 for the
 * bus array dimensions (matches msfa N). */

#include "fm_core.h"
#include "fm_common.h"
#include "fm_exp2.h"
#include "fm_op_kernel.h"

#if FM_N != 64
#error "FmCoreState bus dimensions assume FM_N == 64"
#endif

typedef struct {
    int ops[6];
} FmAlgDesc;

static const FmAlgDesc fm_algorithms[32] = {
    {{0xc1, 0x11, 0x11, 0x14, 0x01, 0x14}}, /* 1 */
    {{0x01, 0x11, 0x11, 0x14, 0xc1, 0x14}}, /* 2 */
    {{0xc1, 0x11, 0x14, 0x01, 0x11, 0x14}}, /* 3 */
    {{0xc1, 0x11, 0x94, 0x01, 0x11, 0x14}}, /* 4 */
    {{0xc1, 0x14, 0x01, 0x14, 0x01, 0x14}}, /* 5 */
    {{0xc1, 0x94, 0x01, 0x14, 0x01, 0x14}}, /* 6 */
    {{0xc1, 0x11, 0x05, 0x14, 0x01, 0x14}}, /* 7 */
    {{0x01, 0x11, 0xc5, 0x14, 0x01, 0x14}}, /* 8 */
    {{0x01, 0x11, 0x05, 0x14, 0xc1, 0x14}}, /* 9 */
    {{0x01, 0x05, 0x14, 0xc1, 0x11, 0x14}}, /* 10 */
    {{0xc1, 0x05, 0x14, 0x01, 0x11, 0x14}}, /* 11 */
    {{0x01, 0x05, 0x05, 0x14, 0xc1, 0x14}}, /* 12 */
    {{0xc1, 0x05, 0x05, 0x14, 0x01, 0x14}}, /* 13 */
    {{0xc1, 0x05, 0x11, 0x14, 0x01, 0x14}}, /* 14 */
    {{0x01, 0x05, 0x11, 0x14, 0xc1, 0x14}}, /* 15 */
    {{0xc1, 0x11, 0x02, 0x25, 0x05, 0x14}}, /* 16 */
    {{0x01, 0x11, 0x02, 0x25, 0xc5, 0x14}}, /* 17 */
    {{0x01, 0x11, 0x11, 0xc5, 0x05, 0x14}}, /* 18 */
    {{0xc1, 0x14, 0x14, 0x01, 0x11, 0x14}}, /* 19 */
    {{0x01, 0x05, 0x14, 0xc1, 0x14, 0x14}}, /* 20 */
    {{0x01, 0x14, 0x14, 0xc1, 0x14, 0x14}}, /* 21 */
    {{0xc1, 0x14, 0x14, 0x14, 0x01, 0x14}}, /* 22 */
    {{0xc1, 0x14, 0x14, 0x01, 0x14, 0x04}}, /* 23 */
    {{0xc1, 0x14, 0x14, 0x14, 0x04, 0x04}}, /* 24 */
    {{0xc1, 0x14, 0x14, 0x04, 0x04, 0x04}}, /* 25 */
    {{0xc1, 0x05, 0x14, 0x01, 0x14, 0x04}}, /* 26 */
    {{0x01, 0x05, 0x14, 0xc1, 0x14, 0x04}}, /* 27 */
    {{0x04, 0xc1, 0x11, 0x14, 0x01, 0x14}}, /* 28 */
    {{0xc1, 0x14, 0x01, 0x14, 0x04, 0x04}}, /* 29 */
    {{0x04, 0xc1, 0x11, 0x14, 0x04, 0x04}}, /* 30 */
    {{0xc1, 0x14, 0x04, 0x04, 0x04, 0x04}}, /* 31 */
    {{0xc4, 0x04, 0x04, 0x04, 0x04, 0x04}} /* 32 */
};

int FmCore_NumOutputs(int algorithm)
{
    int count = 0;
    int i;
    for (i = 0; i < 6; i++) {
        if ((fm_algorithms[algorithm].ops[i] & 7) == FM_OUT_BUS_ADD) {
            count++;
        }
    }
    return count;
}

void FmCore_Render(FmCoreState *st, int32_t *output, FmOp *params,
                   int algorithm, int32_t *fb_buf, int32_t feedback_gain)
{
    const int32_t kLevelThresh = 1120;
    FmAlgDesc alg = fm_algorithms[algorithm];
    int has_contents[3] = {1, 0, 0};
    int op;
    for (op = 0; op < 6; op++) {
        int flags = alg.ops[op];
        int add = (flags & FM_OUT_BUS_ADD) != 0;
        FmOp *param = &params[op];
        int inbus = (flags >> 4) & 3;
        int outbus = flags & 3;
        int32_t *outptr = (outbus == 0) ? output : st->bus[outbus - 1];
        int32_t gain1 = param->gain_out;
        int32_t gain2 = FmExp2_Lookup(param->level_in - (14 * (1 << 24)));
        param->gain_out = gain2;

        if (gain1 >= kLevelThresh || gain2 >= kLevelThresh) {
            if (!has_contents[outbus]) {
                add = 0;
            }
            if (inbus == 0 || !has_contents[inbus]) {
                if ((flags & 0xc0) == 0xc0 && feedback_gain < 16) {
                    FmOpKernel_ComputeFb(outptr, param->phase, param->freq,
                                         gain1, gain2,
                                         fb_buf, (int)feedback_gain, add);
                } else {
                    FmOpKernel_ComputePure(outptr, param->phase, param->freq,
                                           gain1, gain2, add);
                }
            } else {
                FmOpKernel_Compute(outptr, st->bus[inbus - 1],
                                   param->phase, param->freq, gain1, gain2,
                                   add);
            }
            has_contents[outbus] = 1;
        } else if (!add) {
            has_contents[outbus] = 0;
        }
        param->phase += param->freq << FM_LG_N;
    }
}

int FmCore_IsCarrier(int algorithm, int op)
{
    return (fm_algorithms[algorithm].ops[op] & FM_OUT_BUS_ADD) != 0;
}
