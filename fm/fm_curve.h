/*
 * Copyright 2012 Google Inc. (algorithm), FM-1 project (C99 port).
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

/* fm_curve.h — integer amp-mod-sens curve used by the voice render path.
 * Separate TU so the curve unit test links only this + fm_exp2. */

#ifndef FM_CURVE_H
#define FM_CURVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Integer stand-in for upstream's double exp() amp-mod-sens curve
 *   pt = exp(sensamp * (0.07/262144) + 12.2),  sensamp in [0, 2^24].
 * Rewritten base-2: pt = 2^(e2), e2 = (sensamp*K>>16) + C0 in Q16 with
 * K = round(0.07/262144 * log2(e) * 2^16) = 1654,
 * C0 = round(12.2 * log2(e) * 2^16) = 1153139. The fractional octave
 * goes through FmExp2_Lookup; the integer part shifts (64-bit holds).
 * Relative error vs libm exp() is < 1% (see fm_curve_test); upstream's
 * own comment calls this curve "mehhh.. needs some real tuning". */
uint32_t FmNote_AmpCurve(uint32_t sensamp);

#ifdef __cplusplus
}
#endif

#endif /* FM_CURVE_H */
