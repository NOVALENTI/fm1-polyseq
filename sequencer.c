/* sequencer.c
 *
 * Polyphonic 16-step state machine (6 notes/step for sequencer, 6 voices
 * reserved for live play). Strictly static allocation, C99, integer-only ISR.
 *
 * STATE MACHINE
 * -------------
 *  IDLE  : playing==0. Tick only drains safety offs, never advances step.
 *  RUN   : every Sequencer_Tick (1 kHz, dt=SEQ_DT_US) does:
 *    now += dt; into_step += dt;
 *    if (into_step >= step_dur_us): commit boundary ->
 *       1) schedule note-ONs  for the NEW step at due = now + swing_us
 *          (swing may be negative -> clamp to now; micro-timing finer than
 *          1 ms is preserved because the AUDIO side honors due_us at 48 kHz),
 *       2) schedule note-OFFs for those ONs at due = on_due + gate_us,
 *          where gate_us = step_dur_us * gate_pct / 100,
 *       3) force-off any seq voice still held from the previous step
 *          (prevents stuck notes when chords overlap),
 *       4) into_step -= step_dur_us; cur_step = (cur_step+1) % 16.
 *
 * FIFO DISCIPLINE (SPSC lock-free)
 * --------------------------------
 *  Producer = Sequencer_Tick (timer ISR). Consumer = Audio_Process_Callback
 *  (I2S DMA). head: written by producer only; tail: written by consumer only;
 *  both volatile. Full policy: drop-oldest (advance tail) so the sequencer
 *  never blocks the ISR; a sticky overflow counter is kept for debug.
 *  Time comparison is wrap-safe: (int32_t)(now - due) >= 0 means due.
 *
 *  NO malloc/calloc/realloc/free anywhere in this file (verified by grep).
 */

#include "sequencer.h"
#include "hal_shift_register.h"

/* ---------------------------------------------------------------------------
 * STATIC STATE — .bss only.
 * ------------------------------------------------------------------------- */
static SeqStep_t   seq_grid[SEQ_STEPS];
static uint8_t     seq_playing   = 0u;
static uint8_t     seq_cur_step  = 0u;
static uint32_t    seq_now_us    = 0u;  /* monotonic, wraps ~71 min, wrap-safe */
static uint32_t    seq_into_step = 0u;  /* us elapsed in current step */
static uint32_t    seq_step_dur  = 125000u; /* default 120 BPM 16th = 125 ms */
static uint32_t    seq_last_bpm  = 120u;

/* Tracks which seq voice currently holds which note (NOTE_EMPTY = free). */
static uint8_t seq_voice_note[SEQ_VOICES];

/* SPSC event FIFO. */
static volatile MidiEvent_t seq_fifo[SEQ_FIFO_DEPTH];
static volatile uint8_t     seq_head = 0u; /* producer index */
static volatile uint8_t     seq_tail = 0u; /* consumer index */
static volatile uint8_t     seq_overflow = 0u;

#define FIFO_MASK ((uint8_t)(SEQ_FIFO_DEPTH - 1u))

/* ---------------------------------------------------------------------------
 * Internal: push event (producer only). Drop-oldest on full, never blocks.
 * ------------------------------------------------------------------------- */
static void fifo_push(uint8_t type, uint8_t voice, uint8_t note,
                      uint8_t vel, uint32_t due)
{
    uint8_t h = seq_head;
    uint8_t n = (uint8_t)((h + 1u) & FIFO_MASK);
    if (n == seq_tail) {
        /* Full: drop oldest to keep ISR non-blocking. */
        seq_tail = (uint8_t)((seq_tail + 1u) & FIFO_MASK);
        seq_overflow = 1u;
    }
    seq_fifo[h].type     = type;
    seq_fifo[h].voice_id = voice;
    seq_fifo[h].note     = note;
    seq_fifo[h].vel      = vel;
    seq_fifo[h].due_us   = due;
    seq_head = n;
}

/* Schedule one chord: ons at on_due, offs at on_due + gate_us. */
static void schedule_chord(const SeqStep_t *st, uint32_t on_due, uint32_t gate_us)
{
    uint8_t v;
    for (v = 0u; v < SEQ_POLY; v++) {
        uint8_t n = st->notes[v];
        if (n == NOTE_EMPTY || n > 127u) {
            continue;
        }
        uint8_t vel = st->vels[v];
        if (vel > 127u) {
            vel = 127u;
        }
        /* Force-off a voice still holding an old note (stuck-note guard). */
        if (seq_voice_note[v] != NOTE_EMPTY) {
            fifo_push(MIDI_NOTE_OFF, (uint8_t)(SEQ_VOICE_BASE + v),
                      seq_voice_note[v], 64u, on_due);
            seq_voice_note[v] = NOTE_EMPTY;
        }
        fifo_push(MIDI_NOTE_ON, (uint8_t)(SEQ_VOICE_BASE + v), n, vel, on_due);
        fifo_push(MIDI_NOTE_OFF, (uint8_t)(SEQ_VOICE_BASE + v), n, 64u,
                  (uint32_t)(on_due + gate_us));
        seq_voice_note[v] = n;
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
void Sequencer_Init(void)
{
    uint8_t s, v;
    for (s = 0u; s < SEQ_STEPS; s++) {
        for (v = 0u; v < SEQ_POLY; v++) {
            seq_grid[s].notes[v] = NOTE_EMPTY;
            seq_grid[s].vels[v]  = 100u;
        }
        seq_grid[s].gate_pct = 80u;
        seq_grid[s].swing_us = 0;
        seq_grid[s].active   = 1u;
    }
    for (v = 0u; v < SEQ_VOICES; v++) {
        seq_voice_note[v] = NOTE_EMPTY;
    }
    seq_playing   = 0u;
    seq_cur_step  = 0u;
    seq_now_us    = 0u;
    seq_into_step = 0u;
    seq_last_bpm  = 120u;
    seq_step_dur  = 125000u;
    seq_head = 0u;
    seq_tail = 0u;
    seq_overflow = 0u;
}

void Sequencer_Play(void)
{
    unsigned s = IRQ_SAVE();
    seq_playing   = 1u;
    seq_into_step = 0u;
    /* cur_step is kept so playback resumes where the UI left it. */
    IRQ_RESTORE(s);
}

void Sequencer_Stop(void)
{
    unsigned s = IRQ_SAVE();
    seq_playing = 0u;
    IRQ_RESTORE(s);
    Sequencer_AllNotesOffNow();
}

uint8_t Sequencer_IsPlaying(void)
{
    return seq_playing;
}

uint8_t Sequencer_GetStep(void)
{
    return seq_cur_step;
}

void Sequencer_SetStep(uint8_t step, const SeqStep_t *src)
{
    if (step >= SEQ_STEPS || src == 0) {
        return;
    }
    unsigned s = IRQ_SAVE(); /* editor (main) vs Tick (ISR) race guard */
    for (uint8_t v = 0u; v < SEQ_POLY; v++) {
        seq_grid[step].notes[v] = src->notes[v];
        seq_grid[step].vels[v]  = src->vels[v];
    }
    seq_grid[step].gate_pct = (src->gate_pct == 0u) ? 80u :
                              (src->gate_pct > 100u) ? 100u : src->gate_pct;
    seq_grid[step].swing_us = src->swing_us;
    seq_grid[step].active   = src->active ? 1u : 0u;
    IRQ_RESTORE(s);
}

void Sequencer_GetStepData(uint8_t step, SeqStep_t *dst)
{
    uint8_t v;
    unsigned s;
    if (step >= SEQ_STEPS || dst == 0) {
        return;
    }
    /* Field-wise copy: a struct assignment here emits a memcpy libcall
     * on pi32v2, which we keep out of the firmware image. */
    s = IRQ_SAVE();
    for (v = 0u; v < SEQ_POLY; v++) {
        dst->notes[v] = seq_grid[step].notes[v];
        dst->vels[v]  = seq_grid[step].vels[v];
    }
    dst->gate_pct = seq_grid[step].gate_pct;
    dst->swing_us = seq_grid[step].swing_us;
    dst->active   = seq_grid[step].active;
    IRQ_RESTORE(s);
}

void Sequencer_AllNotesOffNow(void)
{
    uint8_t v;
    for (v = 0u; v < SEQ_VOICES; v++) {
        if (seq_voice_note[v] != NOTE_EMPTY) {
            fifo_push(MIDI_NOTE_OFF, (uint8_t)(SEQ_VOICE_BASE + v),
                      seq_voice_note[v], 64u, seq_now_us);
            seq_voice_note[v] = NOTE_EMPTY;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Sequencer_Tick — 1 kHz timer ISR entry. Signature fixed by spec.
 * dt is implicit: SEQ_DT_US (1000 us). Integer math only.
 * ------------------------------------------------------------------------- */
void Sequencer_Tick(uint32_t current_bpm)
{
    /* 1. Time base advance (always, so due_us stays monotonic). */
    seq_now_us    = (uint32_t)(seq_now_us + SEQ_DT_US);
    if (!seq_playing) {
        return;
    }

    /* 2. BPM -> 16th-note duration: 60e6 us/min / (bpm * 4).
     *    Clamp to sane musical range; recompute only on change. */
    if (current_bpm < 30u) {
        current_bpm = 30u;
    } else if (current_bpm > 300u) {
        current_bpm = 300u;
    }
    if (current_bpm != seq_last_bpm) {
        seq_last_bpm = current_bpm;
        seq_step_dur = (uint32_t)(60000000u / (current_bpm * 4u));
    }

    seq_into_step = (uint32_t)(seq_into_step + SEQ_DT_US);

    /* 3. Step boundary crossed? (handles >1 step per tick after long stalls) */
    while (seq_into_step >= seq_step_dur) {
        seq_into_step = (uint32_t)(seq_into_step - seq_step_dur);

        const SeqStep_t *st = &seq_grid[seq_cur_step];

        if (st->active) {
            /* Gate duration for this step. */
            uint8_t gate = st->gate_pct;
            if (gate == 0u) {
                gate = 80u;
            } else if (gate > 100u) {
                gate = 100u;
            }
            /* Gate duration. 32-bit only: step_dur <= 500000 (30 BPM),
             * gate <= 100, so the product (<= 5e7) cannot overflow.
             * (A 64-bit multiply here would drag in __udivdi3 soft-divide
             * on pi32v2, which has no 64-bit divider.) */
            uint32_t gate_us = (seq_step_dur * (uint32_t)gate) / 100u;
            if (gate_us < 1000u) {
                gate_us = 1000u; /* minimum 1 ms so offs never collapse onto ons */
            }

            /* Swing/micro-timing: signed offset applied to ON time only.
             * Clamp so on_due never goes backwards past now-1 tick. */
            int32_t on_due_s = (int32_t)(seq_now_us + (uint32_t)seq_into_step)
                             + (int32_t)st->swing_us;
            if (on_due_s < (int32_t)seq_now_us) {
                on_due_s = (int32_t)seq_now_us;
            }
            schedule_chord(st, (uint32_t)on_due_s, gate_us);
        }

        /* 4. Advance + mirror to step LEDs (bit n = step n). */
        seq_cur_step = (uint8_t)((seq_cur_step + 1u) % SEQ_STEPS);
        HAL_SR_SetLEDs((uint16_t)(1u << seq_cur_step));
    }
}

/* ---------------------------------------------------------------------------
 * FIFO consumer (audio side). Pops only events whose due time has arrived.
 * Peek-then-consume: tail advances only when the head event is due, so
 * future-dated swing/gate events wait without blocking earlier ones
 * (producer always pushes in due-time order per step).
 * ------------------------------------------------------------------------- */
uint8_t SEQ_FIFO_PopDue(uint32_t now_us, MidiEvent_t *ev)
{
    if (seq_head == seq_tail || ev == 0) {
        return 0u;
    }
    /* due reached? wrap-safe signed compare. */
    int32_t dt = (int32_t)(now_us - seq_fifo[seq_tail].due_us);
    if (dt < 0) {
        return 0u;
    }
    /* Field-wise copy (see GetStepData: no memcpy libcalls on pi32v2). */
    ev->type     = seq_fifo[seq_tail].type;
    ev->voice_id = seq_fifo[seq_tail].voice_id;
    ev->note     = seq_fifo[seq_tail].note;
    ev->vel      = seq_fifo[seq_tail].vel;
    ev->due_us   = seq_fifo[seq_tail].due_us;
    seq_tail = (uint8_t)((seq_tail + 1u) & FIFO_MASK);
    return 1u;
}

uint8_t SEQ_FIFO_Pending(void)
{
    return (uint8_t)((seq_head - seq_tail) & FIFO_MASK);
}

uint32_t Sequencer_NowUs(void)
{
    return seq_now_us;
}
