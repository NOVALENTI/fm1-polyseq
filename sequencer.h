#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdint.h>
#include "bsp_config.h"

/* Polyphony split: 12-voice FM engine, 0..5 = sequencer chords, 6..11 = live. */
#define SEQ_STEPS      16
#define SEQ_POLY       6
#define SEQ_VOICES     6
#define SEQ_VOICE_BASE 0u
#define LIVE_VOICE_BASE 6u

#define NOTE_EMPTY     0xFFu
#define SEQ_FIFO_DEPTH 128u  /* must be power of two */

#define MIDI_NOTE_ON   0x90u
#define MIDI_NOTE_OFF  0x80u

/* One sequencer step: chord + performance params. All static, no heap. */
typedef struct {
    uint8_t  notes[SEQ_POLY]; /* MIDI 0..127, NOTE_EMPTY = unused slot */
    uint8_t  vels[SEQ_POLY];  /* 0..127 per-note velocity */
    uint8_t  gate_pct;        /* 1..100: note-off at gate% of step duration */
    int16_t  swing_us;        /* micro-timing offset, signed us, +/- step/2 */
    uint8_t  active;          /* 1 = step sounds, 0 = muted/rest */
} SeqStep_t;

/* Timestamped synth event for the SPSC FIFO (timer ISR -> audio DMA). */
typedef struct {
    uint8_t  type;      /* MIDI_NOTE_ON / MIDI_NOTE_OFF */
    uint8_t  voice_id;  /* 0..5 sequencer (live path uses 6..11 separately) */
    uint8_t  note;      /* MIDI note */
    uint8_t  vel;       /* velocity (off events: release vel, usually 0/64) */
    uint32_t due_us;    /* monotonic sequencer clock: fire when audio time >= due */
} MidiEvent_t;

void Sequencer_Init(void);
void Sequencer_Play(void);
void Sequencer_Stop(void);              /* panic-off: silences seq voices */
uint8_t Sequencer_IsPlaying(void);
uint8_t Sequencer_GetStep(void);        /* for LED display */

/* Required entry point: call from 1 kHz timer ISR with current BPM.
 * Advances time, handles step boundaries + gate-offs, pushes timestamped
 * MIDI events into the lock-free FIFO. Integer math only, no float. */
void Sequencer_Tick(uint32_t current_bpm);

/* Consumer side (audio callback). Returns 1 + fills *ev if an event is due
 * at-or-before now_us, else 0. Lock-free SPSC: single ISR producer. */
uint8_t SEQ_FIFO_PopDue(uint32_t now_us, MidiEvent_t *ev);
uint8_t SEQ_FIFO_Pending(void);
/* Shared monotonic clock (us) for aligning audio block_end with due_us.
 * Both sequencer and audio clocks start at 0 at boot and advance in real
 * time; use this to avoid domain skew after init order changes. */
uint32_t Sequencer_NowUs(void);

/* Editor access (main loop, NOT ISR): configure grid contents. */
void Sequencer_SetStep(uint8_t step, const SeqStep_t *s);
void Sequencer_GetStepData(uint8_t step, SeqStep_t *s);
void Sequencer_AllNotesOffNow(void);    /* push immediate offs for seq voices */

#endif /* SEQUENCER_H */
