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

/* fm_op_kernel.h — C99 port of Dexed/msfa FmOpKernel (scalar path only;
 * the Android NEON path does not exist on pi32v2). gain1/gain2 are the
 * linear gain ramp endpoints over FM_N samples; `add` selects accumulate
 * vs overwrite. fb_buf holds 2 feedback state samples across calls. */

#ifndef FM_OP_KERNEL_H
#define FM_OP_KERNEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: upstream's `struct FmOpParams` (per-operator render params) is
 * introduced with the fm_core port, which is its only consumer. */

void FmOpKernel_Compute(int32_t *output, const int32_t *input,
                        int32_t phase0, int32_t freq,
                        int32_t gain1, int32_t gain2, int add);

void FmOpKernel_ComputePure(int32_t *output, int32_t phase0, int32_t freq,
                            int32_t gain1, int32_t gain2, int add);

void FmOpKernel_ComputeFb(int32_t *output, int32_t phase0, int32_t freq,
                          int32_t gain1, int32_t gain2,
                          int32_t *fb_buf, int fb_shift, int add);

#ifdef __cplusplus
}
#endif

#endif /* FM_OP_KERNEL_H */
