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

#include "fm_curve.h"
#include "fm_exp2.h"

uint32_t FmNote_AmpCurve(uint32_t sensamp)
{
    uint32_t e16 = (uint32_t)(((uint64_t)sensamp * 1654u >> 16)) + 1153139u;
    uint32_t qint = e16 >> 16;
    uint32_t frac24 = (e16 & 0xFFFFu) << 8;
    uint32_t mant = (uint32_t)FmExp2_Lookup((int32_t)frac24);
    return (uint32_t)(((uint64_t)mant << qint) >> 24);
}
