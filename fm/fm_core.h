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

/* fm_core.h — C99 port of Dexed/msfa FmCore (fm_core.h/fm_core.cc).
 * Devirtualized: single render function. The two N-sample scratch buses
 * live in caller-provided FmCoreState (reentrant, statically allocatable)
 * instead of a C++ member. Names are Fm-prefixed throughout: the original
 * `struct FmOpParams` / `FmAlgorithm` identifiers are kept free so
 * bit-exact cross-check builds can include both headers. */

#ifndef FM_CORE_H
#define FM_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-operator render params (mirrors upstream FmOpParams layout). */
typedef struct {
    int32_t level_in;
    int32_t gain_out;
    int32_t freq;
    int32_t phase;
} FmOp;

/* Render scratch buses (upstream AlignedBuf<int32_t,N> buf_[2]). */
typedef struct {
    int32_t bus[2][64];
} FmCoreState;

/* Bus routing flags (mirror upstream FmOperatorFlags values). */
#define FM_OUT_BUS_ONE  (1 << 0)
#define FM_OUT_BUS_TWO  (1 << 1)
#define FM_OUT_BUS_ADD  (1 << 2)
#define FM_IN_BUS_ONE   (1 << 4)
#define FM_IN_BUS_TWO   (1 << 5)
#define FM_FB_IN        (1 << 6)
#define FM_FB_OUT       (1 << 7)

/* Render one FM_N-sample block. output need not be zeroed (bus tracking
 * handles first-writer-overwrite). fb_buf holds 2 persistent feedback
 * samples; feedback_gain maps like upstream feedback_shift (<16 = fb). */
void FmCore_Render(FmCoreState *st, int32_t *output, FmOp *params,
                   int algorithm, int32_t *fb_buf, int32_t feedback_gain);

/* True when op is a carrier (writes the output bus) in algorithm. */
int FmCore_IsCarrier(int algorithm, int op);

/* Number of output-bus writers in algorithm (gain staging helper). */
int FmCore_NumOutputs(int algorithm);

#ifdef __cplusplus
}
#endif

#endif /* FM_CORE_H */
