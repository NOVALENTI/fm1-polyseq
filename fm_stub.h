#ifndef FM_STUB_H
#define FM_STUB_H

/* fm_stub.h — flat C-linkage shim over the Dexed/MSFA 6-op FM engine.
 * Real firmware links these to the engine (extern "C" wrappers, or the
 * C99 port scoped below). Voice split: 0..5 sequencer, 6..11 live
 * keyboard. Engine MUST NOT steal across the split (caller owns voice
 * allocation).
 *
 * C99 PORT SCOPE (audited against asb2m10/dexed Source/msfa; Apache-2.0
 * Google license attribution must be retained on port):
 *  CONVERT (~1400 lines, fixed-point int32 DSP, ZERO heap in core):
 *   dx7note, env, fm_op_kernel, fm_core, lfo, sin, freqlut, pitchenv,
 *   controllers, exp2. Renders int32 Q-format buffers (fm_core render
 *   takes int32_t*); our audio glue already scales to float/s16.
 *  MECHANICAL TRANSFORMS: classes -> structs + explicit context pointers;
 *   min/max + AlignedBuf templates -> static inline fns / aligned static
 *   arrays; the 2 FmCore virtuals (dtor + render, single EngineMkI
 *   subclass) -> direct calls. `double` appears ONLY in boot-time
 *   init(sample_rate) paths and one droppable MTS field — never in render.
 *  DROP: tuning.cc (STL shared_ptr/exceptions/SCL-KBM; keep standard
 *   tuning via freqlut), all EngineMkI/UI/plugin layers. Voice stealing
 *   is REPLACED by 12 static Dx7Note instances with the fixed 6+6 split.
 *  TABLES: sine/exp2 LUTs are init()-filled at boot -> keep boot-time
 *   fill into static RAM, or const-generate into NOR flash if RAM pinches.
 *  MAC HOOKS: fm_op_kernel inner loop + env MACs are the Type-III
 *   dual-MAC asm targets (see audio_core.c Region B). */

#include <stdint.h>

#define FM_NUM_VOICES 12u
#define FM_SEQ_VOICES 6u

void FM_Init(uint32_t sample_rate);
void FM_NoteOn(uint8_t voice_id, uint8_t midi_note, uint8_t velocity);
void FM_NoteOff(uint8_t voice_id);
/* Renders num_frames stereo interleaved? No — split buffers, float -1..1. */
void FM_Render(float *left_out, float *right_out, uint16_t num_frames);

#endif /* FM_STUB_H */
