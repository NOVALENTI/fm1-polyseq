#ifndef AUDIO_CORE_H
#define AUDIO_CORE_H

#include <stdint.h>

void Audio_Init(void);

/* DAC DMA callback: fill `output_buffer` (interleaved stereo LRLR...) with
 * num_samples FRAMES. Called from the DAC data-handler context. */
void Audio_Process_Callback(float *output_buffer, uint16_t num_samples);

/* Live-play path (main loop): uses voices 6..11, never touches 0..5. */
void Audio_LiveNoteOn(uint8_t midi_note, uint8_t velocity);
void Audio_LiveNoteOff(uint8_t midi_note);

#endif /* AUDIO_CORE_H */
