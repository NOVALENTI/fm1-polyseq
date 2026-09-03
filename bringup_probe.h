#ifndef BRINGUP_PROBE_H
#define BRINGUP_PROBE_H

#include <stdint.h>
#include "bsp_config.h"
#include "debug_midi.h" /* for Debug_PutcFn */

/* Matrix-discovery helpers for reverse-engineering the FM-1 front panel
 * WITHOUT a multimeter. Call these from the main loop (NOT from an ISR),
 * paced ~50-100 ms apart, with putc_fn bound to the UART TX byte sink.
 *
 *  Probe_ShiftOne(bit): shifts the one-hot word (1u << bit) into the
 *    cascaded 595s, latches it, samples the key-column inputs, and logs
 *    "P<bb> IN=<hh>\n" where <bb> is the shift bit (00-15, decimal) and
 *    <hh> is the sampled column field (hex). Sweep bit 0..15: bits that
 *    make columns respond are ROW drives; bits that light an LED (watch
 *    the panel) are LED drives. Bits doing neither are spare/NC.
 *  Probe_LEDOne(led): stages the HAL LED mask (1u << led) and logs
 *    "LED<bb>\n" so you can correlate the visible LED with the step bit.
 *
 * No heap, no float, bounded work. Safe to run before the FM engine and
 * sequencer are started. */
void Probe_ShiftOne(uint8_t bit, Debug_PutcFn putc_fn);
void Probe_LEDOne(uint8_t led, Debug_PutcFn putc_fn);

/* Non-blocking full sweep, paced by the caller (~50-100 ms per call).
 * Each call emits exactly one line: sub-steps 0..15 run Probe_ShiftOne,
 * sub-steps 16..31 run Probe_LEDOne, then the position wraps. Static
 * position state, no heap. */
void Probe_SweepStep(Debug_PutcFn putc_fn);

/* Convenience: 32 paced sub-steps back-to-back (for scripted UART dumps). */
void Probe_SweepOnce(Debug_PutcFn putc_fn);

#endif /* BRINGUP_PROBE_H */
