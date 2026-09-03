#ifndef DEBUG_MIDI_H
#define DEBUG_MIDI_H

#include <stdint.h>

/* UART MIDI-dump helper for hardware bring-up (optional, not in default build).
 * Validates sequencer timing over UART BEFORE enabling the FM engine.
 * No stdio dependency: caller supplies a putchar-like byte sink. No heap. */
typedef void (*Debug_PutcFn)(char c);

void Debug_DumpPendingEvents(Debug_PutcFn putc_fn);

#endif /* DEBUG_MIDI_H */
