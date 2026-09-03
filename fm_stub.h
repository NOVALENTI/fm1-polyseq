#ifndef FM_STUB_H
#define FM_STUB_H

/* fm_stub.h — flat C-linkage shim over the Dexed/MSFA 6-op FM engine.
 * Real firmware links these to the C++ engine via extern "C" wrappers.
 * Voice split: 0..5 sequencer, 6..11 live keyboard. Engine MUST NOT steal
 * across the split (caller owns voice allocation). */

#include <stdint.h>

#define FM_NUM_VOICES 12u
#define FM_SEQ_VOICES 6u

void FM_Init(uint32_t sample_rate);
void FM_NoteOn(uint8_t voice_id, uint8_t midi_note, uint8_t velocity);
void FM_NoteOff(uint8_t voice_id);
/* Renders num_frames stereo interleaved? No — split buffers, float -1..1. */
void FM_Render(float *left_out, float *right_out, uint16_t num_frames);

#endif /* FM_STUB_H */
