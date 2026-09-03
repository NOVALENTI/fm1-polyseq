/* audio_core.c
 *
 * Bridge: sequencer FIFO -> Dexed 6-op FM engine -> audio DMA buffer.
 *
 * SDK HOOKUP (verified in fw-AC79_AIoT_SDK, include_lib/driver/cpu/wl82):
 *  Internal DAC path (asm/dac.h): dac_open(&pd) -> dac_set_sample_rate()
 *    -> dac_set_data_handler(priv, handler) -> dac_on(). The handler has
 *    signature handler(void *priv, u8 *data, int len) and fires every
 *    sr_points samples ("多少个采样点进一次中断"); Audio_Process_Callback
 *    below IS that handler body (adapt float stereo to the s16le buffer
 *    at the glue layer: len_bytes = frames * 2ch * 2B).
 *  External DAC path, e.g. CS4344 (asm/iis.h): iis_open(&pd, index) with
 *    port_sel = IIS_PORTA/PORTC/PORTG, channel_out, data_width, mclk_output
 *    and f32e frame clocks, then iis_channel_on(). Same sr_points pacing.
 *  Either way BLOCK_FRAMES maps 1:1 to the SDK's sr_points field; keep it
 *    128 (≈2.66 ms @48 kHz) unless the DAC driver demands otherwise.
 *
 * EXECUTION CONTEXT: runs in DMA data-handler context at 48 kHz /
 * 128 frames. Bounded, heap-free, never blocks on the timer ISR. MIDI
 * consumption is a bounded drain (MAX_EVENTS_PER_BLOCK) so a pathological
 * FIFO burst cannot starve render.
 *
 * SAMPLE CLOCK: audio_now_us advances by frames*1e6/SAMPLE_RATE using integer
 * math (128 @48k = 2666 us + fractional carry). due_us from the sequencer
 * shares the same monotonic domain (both advance in real microseconds), so
 * swing (<1 ms) and gate timing land with sub-block precision by firing
 * events at the exact frame index instead of block boundaries.
 *
 * NOTE: output is interleaved stereo floats [L R L R ...], length = 2*frames.
 * The DAC/IIS glue (not here) converts to the s16le DMA words the SDK
 * data handler expects (see SDK HOOKUP above).
 *
 * pi32v2 DSP OPTIMIZATION HOOKS (marked TODO(pi32v2) below):
 *  - Region A (event drain): already scalar/integer; keep in C.
 *  - Region B (FM_Render internals: phase accum, 6-op feedback, envelope
 *    MACs): insert Type-III dual-MAC + repeat-loop asm in the Dexed voice
 *    loop in a later pass. This file keeps a clean call boundary for that.
 *  - Region C (interleave/mono->stereo + soft-clip): ideal for a 2x unrolled
 *    MAC/saturate asm kernel; C fallback provided here.
 */

#include "audio_core.h"
#include "bsp_config.h"
#include "sequencer.h"
#include "fm_stub.h"

/* Bounded work per DMA block: worst case 2 events/note * 6 notes = 12/step,
 * plus offs; 32 covers a full chord change + live traffic with headroom. */
#define MAX_EVENTS_PER_BLOCK 32u

/* Live-voice allocator state (voices 6..11). Static, no heap. */
static uint8_t live_note[FM_SEQ_VOICES]; /* index 0..5 -> voice 6..11 */
static uint8_t live_next = 0u;           /* round-robin cursor */

/* Monotonic audio clock (us) + fractional carry for exact 2666.666... pacing.
 * 32-bit quotient/remainder form: no 64-bit divide anywhere in this file. */
static uint32_t audio_now_us = 0u;
static uint32_t audio_carry = 0u; /* leftover 1e6ths... in units of (num*1e6 % rate) */

/* Branchless-ish saturate to [-1,1] via IEEE-754 bit inspection.
 * A plain `x > 1.0f` comparison emits __gtsf2/__ltsf2 soft-float libcalls
 * on pi32v2; this integer-only form keeps the DMA callback libcall-free.
 * |x| <= 1.0 (bits, sign-cleared, <= 0x3F800000) passes through;
 * anything larger (incl. Inf/NaN) saturates by sign bit. */
static float fast_clip(float x)
{
    union { float f; uint32_t u; } v;
    v.f = x;
    if ((v.u & 0x7FFFFFFFu) <= 0x3F800000u) {
        return x;
    }
    return (v.u & 0x80000000u) ? -1.0f : 1.0f;
}

void Audio_Init(void)
{
    uint8_t i;
    for (i = 0u; i < FM_SEQ_VOICES; i++) {
        live_note[i] = NOTE_EMPTY;
    }
    live_next = 0u;
    audio_now_us = 0u;
    audio_carry = 0u;
    FM_Init(SAMPLE_RATE);
}

void Audio_LiveNoteOn(uint8_t midi_note, uint8_t velocity)
{
    uint8_t slot;
    if (midi_note > 127u) {
        return;
    }
    if (velocity > 127u) {
        velocity = 127u;
    }
    /* Round-robin steal within live split only — never touches voices 0..5. */
    slot = live_next;
    live_next = (uint8_t)((live_next + 1u) % FM_SEQ_VOICES);
    if (live_note[slot] != NOTE_EMPTY) {
        FM_NoteOff((uint8_t)(LIVE_VOICE_BASE + slot));
    }
    live_note[slot] = midi_note;
    FM_NoteOn((uint8_t)(LIVE_VOICE_BASE + slot), midi_note, velocity);
}

void Audio_LiveNoteOff(uint8_t midi_note)
{
    uint8_t i;
    for (i = 0u; i < FM_SEQ_VOICES; i++) {
        if (live_note[i] == midi_note) {
            FM_NoteOff((uint8_t)(LIVE_VOICE_BASE + i));
            live_note[i] = NOTE_EMPTY;
        }
    }
}

void Audio_Process_Callback(float *output_buffer, uint16_t num_samples)
{
    uint16_t f;
    uint8_t  n;
    MidiEvent_t ev;

    if (output_buffer == 0 || num_samples == 0u) {
        return;
    }
    if (num_samples > BLOCK_FRAMES) {
        num_samples = BLOCK_FRAMES; /* clamp: DMA contract is <=128 frames */
    }

    /* ---- Region A: drain due sequencer events (bounded, integer-only). ----
     * Events carry due_us; fire everything due at-or-before the END of this
     * block so gate/swing edges inside the block are honored, not quantized
     * to block starts. Order is preserved (FIFO is due-ordered per step). */
    {
        /* Block end time: now + block duration. 32-bit only:
         * num_samples <= 128, so num*1e6 (<= 1.28e8) cannot overflow. */
        uint32_t adv = ((uint32_t)num_samples * 1000000u) / SAMPLE_RATE;
        uint32_t block_end = audio_now_us + adv;

        for (n = 0u; n < MAX_EVENTS_PER_BLOCK; n++) {
            if (!SEQ_FIFO_PopDue(block_end, &ev)) {
                break;
            }
            /* Sequencer owns voices 0..5 only; ignore anything else. */
            if (ev.voice_id >= FM_SEQ_VOICES) {
                continue;
            }
            if (ev.type == MIDI_NOTE_ON) {
                FM_NoteOn(ev.voice_id, ev.note, ev.vel);
            } else {
                FM_NoteOff(ev.voice_id);
            }
        }
    }

    /* ---- Region B: FM render. Heavy 6-op math lives inside FM_Render. ----
     * TODO(pi32v2): inside the Dexed voice loop, replace the C inner
     * product (carrier + 5 modulators + feedback + envelope MACs) with
     * Type-III dual-MAC inline asm, e.g.:
     *   __asm__ volatile ("mac ... " : "+r"(acc) : "r"(m), "r"(e) : ...);
     * with a loop-repeat prefix over operators. Keep the C path under
     * #ifndef PI32V2_MAC for host builds. Do NOT move heap/float into ISRs
     * beyond this already-audio-context call. */
    {
        /* Render into a scratch split pair then interleave, so FM_Render's
         * signature (L/R split) stays stable while DMA wants interleaved.
         * Static scratch avoids stack overflow on small ISR stacks. */
        static float tmp_l[BLOCK_FRAMES];
        static float tmp_r[BLOCK_FRAMES];
        FM_Render(tmp_l, tmp_r, num_samples);

        /* ---- Region C: interleave + saturate. ----
         * TODO(pi32v2): 2x-unrolled saturating-store kernel with dual-MAC
         * for a final width/gain stage, e.g. acc = l*g + r*g with saturation.
         * C fallback below is bit-exact for host verification. */
        for (f = 0u; f < num_samples; f++) {
            output_buffer[2u * f]     = fast_clip(tmp_l[f]);
            output_buffer[2u * f + 1u] = fast_clip(tmp_r[f]);
        }
    }

    /* ---- Advance audio clock. Quotient/remainder form keeps long-term
     * pacing exact (128 @48k = 2666 + 32000/48000 us) with 32-bit math. -- */
    {
        uint32_t prod = (uint32_t)num_samples * 1000000u; /* <= 1.28e8 */
        uint32_t base = prod / SAMPLE_RATE;
        audio_carry += prod % SAMPLE_RATE;
        if (audio_carry >= SAMPLE_RATE) {
            audio_carry -= SAMPLE_RATE;
            base++;
        }
        audio_now_us += base;
    }
}
