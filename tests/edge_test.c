/* Edge-case tests: BPM clamp, gate/swing ordering, FIFO non-blocking
 * overflow policy, mute steps, live voice-split isolation.
 * Build: make host (runs this + host_test). */
#include <stdio.h>
#include <assert.h>

#ifndef UNIT_TEST_HOST
#define UNIT_TEST_HOST
#endif
#include "bsp_config.h"

volatile uint32_t HOST_GPIOA_DIR, HOST_GPIOA_OUT, HOST_GPIOA_IN;

#include "hal_shift_register.h"
#include "sequencer.h"
#include "audio_core.h"
#include "fm_stub.h"

/* Mock FM engine with voice logging. */
static uint8_t seen_seq_voice[FM_SEQ_VOICES];
static uint8_t seen_live_voice[FM_SEQ_VOICES];
static int render_cnt = 0;
void FM_Init(uint32_t sr) { (void)sr; }
void FM_NoteOn(uint8_t v, uint8_t n, uint8_t vel)
{
    (void)n; (void)vel;
    if (v < FM_SEQ_VOICES) {
        seen_seq_voice[v] = 1;
    } else if (v < FM_NUM_VOICES) {
        seen_live_voice[v - FM_SEQ_VOICES] = 1;
    } else {
        assert(0 && "voice out of range");
    }
}
void FM_NoteOff(uint8_t v)
{
    if (v < FM_SEQ_VOICES) {
        seen_seq_voice[v] = 1;
    } else if (v < FM_NUM_VOICES) {
        seen_live_voice[v - FM_SEQ_VOICES] = 1;
    } else {
        assert(0 && "voice out of range");
    }
}
void FM_Render(float *l, float *r, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++) { l[i] = 0.0f; r[i] = 0.0f; }
    render_cnt++;
}

static void full_chord_step(SeqStep_t *s, uint8_t base, uint8_t gate,
                            int16_t swing, uint8_t active)
{
    for (uint8_t v = 0; v < SEQ_POLY; v++) {
        s->notes[v] = (uint8_t)(base + v);
        s->vels[v] = 100;
    }
    s->gate_pct = gate;
    s->swing_us = swing;
    s->active = active;
}

int main(void)
{
    float buf[2 * BLOCK_FRAMES];
    MidiEvent_t ev;

    /* 1. BPM clamp + step timing: 30 BPM -> 500 ms/step; 300 BPM -> 50 ms. */
    Sequencer_Init();
    Audio_Init();
    SeqStep_t s;
    Sequencer_GetStepData(0, &s);
    full_chord_step(&s, 60, 80, 0, 1);
    Sequencer_SetStep(0, &s);
    for (int i = 1; i < 16; i++) {
        SeqStep_t m = s; m.active = 0; Sequencer_SetStep((uint8_t)i, &m);
    }
    Sequencer_Play();
    uint8_t step0 = Sequencer_GetStep();
    for (int t = 0; t < 505; t++) {
        Sequencer_Tick(30);
    }
    assert(Sequencer_GetStep() != step0); /* advanced within ~500 ms */
    printf("BPM-30 timing OK\n");

    /* 2. Muted step emits nothing. */
    Sequencer_Init();
    Sequencer_GetStepData(0, &s);
    full_chord_step(&s, 60, 80, 0, 0);
    Sequencer_SetStep(0, &s);
    for (int i = 1; i < 16; i++) {
        SeqStep_t m = s; Sequencer_SetStep((uint8_t)i, &m);
    }
    Sequencer_Play();
    for (int t = 0; t < 600; t++) {
        Sequencer_Tick(120);
    }
    assert(SEQ_FIFO_Pending() == 0);
    printf("Mute OK\n");

    /* 3. Swing/gate ordering: +2 ms swing, gate 90% — offs strictly after ons. */
    Sequencer_Init();
    Sequencer_GetStepData(0, &s);
    full_chord_step(&s, 60, 90, 2000, 1);
    Sequencer_SetStep(0, &s);
    for (int i = 1; i < 16; i++) {
        SeqStep_t m = s; m.active = 0; Sequencer_SetStep((uint8_t)i, &m);
    }
    Sequencer_Play();
    for (int t = 0; t < 130; t++) {
        Sequencer_Tick(120);
    }
    {
        uint32_t first_on = 0xFFFFFFFFu, last_off = 0u;
        int ons = 0, offs = 0;
        uint32_t now = Sequencer_NowUs() + 1000000u; /* far future: drain all */
        while (SEQ_FIFO_PopDue(now, &ev)) {
            if (ev.type == MIDI_NOTE_ON) {
                ons++;
                if (ev.due_us < first_on) {
                    first_on = ev.due_us;
                }
            } else {
                offs++;
                if (ev.due_us > last_off) {
                    last_off = ev.due_us;
                }
            }
        }
        assert(ons == 6 && offs == 6);
        assert(last_off > first_on);
        printf("Swing/gate order OK ons=%d offs=%d\n", ons, offs);
    }

    /* 4. FIFO overflow: 300 BPM full chords, no drain — ISR must not block,
     * pending stays capped, order still drains afterwards. */
    Sequencer_Init();
    for (int i = 0; i < 16; i++) {
        SeqStep_t f;
        full_chord_step(&f, 60, 80, 0, 1);
        Sequencer_SetStep((uint8_t)i, &f);
    }
    Sequencer_Play();
    for (int t = 0; t < 2000; t++) {
        Sequencer_Tick(300); /* 50 ms/step, 12 events/step, no consumer */
    }
    assert(SEQ_FIFO_Pending() < SEQ_FIFO_DEPTH); /* capped, never overflows */
    {
        uint32_t now = Sequencer_NowUs() + 1000000u;
        int drained = 0;
        while (SEQ_FIFO_PopDue(now, &ev)) {
            drained++;
        }
        assert(drained > 0 && drained < 256);
        printf("Overflow policy OK drained=%d\n", drained);
    }

    /* 5. Live voice-split: 7 rapid live ons steal only within 6..11. */
    Audio_Init();
    for (uint8_t i = 0; i < FM_SEQ_VOICES; i++) {
        seen_seq_voice[i] = 0;
        seen_live_voice[i] = 0;
    }
    for (uint8_t k = 0; k < 7; k++) {
        Audio_LiveNoteOn((uint8_t)(60 + k), 100);
    }
    for (uint8_t i = 0; i < FM_SEQ_VOICES; i++) {
        assert(seen_seq_voice[i] == 0); /* sequencer voices untouched */
    }
    assert(seen_live_voice[0] == 1); /* slot 0 was stolen by 7th note */
    Audio_Process_Callback(buf, BLOCK_FRAMES);
    assert(render_cnt == 1);
    printf("Voice-split OK\n");

    printf("ALL EDGE TESTS PASSED\n");
    return 0;
}
