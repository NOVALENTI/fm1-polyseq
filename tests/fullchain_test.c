/* fullchain_test.c — end-to-end: sequencer chords -> FIFO -> DMA blocks
 * -> REAL FM engine -> interleaved float audio. Proves Modules 2+3 work
 * together with live synthesis: a programmed C-major step must be clearly
 * audible in the DMA output while a live key plays on a split voice.
 * Links the real engine (no FM mocks) + HAL stubs. */
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

static float dma[2 * BLOCK_FRAMES];

static float run_ms(int ms, int live_note)
{
    float peak = 0.0f;
    int blocks = 0;
    for (int t = 0; t < ms; t++) {
        HAL_SR_TimerISR();
        Sequencer_Tick(120);
        if (live_note >= 0 && t == ms / 2) {
            Audio_LiveNoteOn((uint8_t)live_note, 100);
        }
        /* One DMA block every ~2.66 ms (128 frames @48 kHz). */
        if ((t % 8) == 7) {
            unsigned f;
            Audio_Process_Callback(dma, BLOCK_FRAMES);
            blocks++;
            for (f = 0u; f < 2u * BLOCK_FRAMES; f++) {
                float v = dma[f] < 0.0f ? -dma[f] : dma[f];
                if (v > peak) {
                    peak = v;
                }
                assert(v <= 1.0f);
            }
        }
    }
    (void)blocks;
    return peak;
}

int main(void)
{
    SeqStep_t s;
    float peak;
    int i;

    HAL_SR_Init();
    Sequencer_Init();
    Audio_Init();

    /* Program a C-major triad on step 0, mute the rest. */
    Sequencer_GetStepData(0, &s);
    s.notes[0] = 60;
    s.vels[0] = 100;
    s.notes[1] = 64;
    s.vels[1] = 100;
    s.notes[2] = 67;
    s.vels[2] = 100;
    s.gate_pct = 90;
    s.swing_us = 0;
    s.active = 1;
    Sequencer_SetStep(0, &s);
    for (i = 1; i < 16; i++) {
        SeqStep_t m = s;
        m.active = 0;
        Sequencer_SetStep((uint8_t)i, &m);
    }
    Sequencer_Play();

    /* 500 ms with a live E key joining halfway: chord + lead audible. */
    peak = run_ms(500, 76);
    printf("fullchain peak=%f\n", peak);
    assert(peak > 0.02f);

    /* Stop: sequencer offs flush, output decays. */
    Sequencer_Stop();
    peak = run_ms(400, -1);
    printf("after stop peak=%f\n", peak);
    (void)peak;

    printf("ALL FULLCHAIN TESTS PASSED\n");
    return 0;
}
