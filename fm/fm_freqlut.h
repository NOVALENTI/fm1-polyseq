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

/* fm_freqlut.h — C99 port of Dexed/msfa Freqlut (freqlut.h/freqlut.cc).
 * Resolves a frequency signal (1.0 in Q24 = 1 octave) to a phase delta.
 * Table depends on sample rate: init at boot (libm), lookup is integer. */

#ifndef FM_FREQLUT_H
#define FM_FREQLUT_H

#include <stdint.h>

void FmFreqlut_Init(double sample_rate);
int32_t FmFreqlut_Lookup(int32_t logfreq);

#endif /* FM_FREQLUT_H */
