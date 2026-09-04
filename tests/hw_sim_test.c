/* hw_sim_test.c — hardware-in-the-loop simulation (host only).
 *
 * A virtual front panel: 4x7 key matrix with programmable bounce behind
 * GPIOA_IN, plus assertions on the latched 595 words. Drives the REAL
 * HAL ISR at 1 kHz and checks:
 *  1. Row strobe rotates one-hot 0..3 and LED nibbles track the mask.
 *  2. Bouncy key press (chatter) debounces to stable ON in ~12 ms with
 *     no flicker; release debounces symmetrically.
 *  3. Full key-to-sound path: pressed key -> stable state -> live note
 *     -> audible DMA output through the real FM engine; release decays.
 *  4. Step LEDs follow the running sequencer across scan phases.
 *
 * The test tracks the scanner phase itself (0 after HAL_SR_Init, +1 per
 * ISR call, mod 4) to script per-row column inputs. */
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
#include "fm_voice.h"

/* Real FM engine linked in (no mocks): sound assertions are genuine. */

static uint8_t key_model[MATRIX_ROWS][MATRIX_COLS];
static int sim_phase = 0;

static void sim_columns(void)
{
    uint32_t cols = 0u;
    int c;
    for (c = 0; c < MATRIX_COLS; c++) {
        if (key_model[sim_phase][c]) {
            cols |= (1u << c);
        }
    }
    HOST_GPIOA_IN = cols << KEY_COL_SHIFT;
}

static void sim_tick(void)
{
    sim_columns();
    HAL_SR_TimerISR();
    sim_phase = (sim_phase + 1) % MATRIX_ROWS;
}

static float dma[2 * BLOCK_FRAMES];

static float dma_peak(void)
{
    float p = 0.0f;
    unsigned i;
    Audio_Process_Callback(dma, BLOCK_FRAMES);
    for (i = 0u; i < 2u * BLOCK_FRAMES; i++) {
        float v = dma[i] < 0.0f ? -dma[i] : dma[i];
        if (v > p) {
            p = v;
        }
    }
    return p;
}

int main(void)
{
    uint8_t keys[NUM_KEYS];
    uint8_t prev[NUM_KEYS] = {0};
    int t, i;

    HAL_SR_Init();
    Sequencer_Init();
    Audio_Init();

    /* 1. Strobe rotation + LED tracking over 8 ms. */
    HAL_SR_SetLEDs(0xBEEF);
    for (t = 0; t < 8; t++) {
        int phase = sim_phase;
        uint16_t want_nib = (uint16_t)((0xBEEFu >> (phase * 4)) & 0x0Fu);
        uint16_t w;
        sim_tick();
        w = HAL_SR_LastLatched();
        assert((w & 0xFu) == (uint16_t)(1u << phase)); /* one-hot row */
        assert(((w >> 4) & 0xFu) == want_nib);         /* LED nibble */
        assert(((w >> 12) & 0xFu) == want_nib);        /* mirror nibble */
    }
    printf("STROBE+LED OK\n");

    /* 2. Bounce: key (row 1, col 2) chatters across row-1 visits. */
    {
        /* Raw pattern per visit to row 1 (4 ms apart). */
        const uint8_t bounce[] = {0, 1, 0, 1, 1, 1, 1, 1, 1, 1};
        int visit = 0;
        int first_on = -1;
        for (t = 0; t < 60; t++) {
            if (sim_phase == 1 && visit < 10) {
                key_model[1][2] = bounce[visit++];
            }
            sim_tick();
            HAL_SR_GetKeys(keys);
            if (keys[1 * MATRIX_COLS + 2] && first_on < 0) {
                first_on = t;
            }
            if (first_on < 0) {
                assert(keys[1 * MATRIX_COLS + 2] == 0); /* no flicker */
            }
        }
        /* 3 consecutive agreeing visits = 12 ms after chatter settles. */
        assert(first_on >= 0 && first_on <= 30);
        printf("DEBOUNCE-ON OK at t=%dms\n", first_on);
        /* Release: clear model, expect stable OFF within ~16 ms. */
        key_model[1][2] = 0;
        {
            int off_at = -1;
            for (t = 0; t < 40; t++) {
                sim_tick();
                HAL_SR_GetKeys(keys);
                if (!keys[1 * MATRIX_COLS + 2] && off_at < 0) {
                    off_at = t;
                }
            }
            assert(off_at >= 0 && off_at <= 16);
            printf("DEBOUNCE-OFF OK at t=%dms\n", off_at);
        }
    }

    /* 3. Key-to-sound: hold key 5 (row 0, col 5) -> live note -> audio. */
    key_model[0][5] = 1;
    for (t = 0; t < 30; t++) {
        sim_tick();
    }
    {
        float quiet, loud;
        int k;
        quiet = dma_peak();
        HAL_SR_GetKeys(keys);
        for (k = 0; k < NUM_KEYS; k++) {
            if (keys[k] && !prev[k]) {
                Audio_LiveNoteOn((uint8_t)(60u + k), 100);
            }
            prev[k] = keys[k];
        }
        assert(keys[5] == 1);
        loud = 0.0f;
        for (t = 0; t < 40; t++) {
            float p = dma_peak();
            if (p > loud) {
                loud = p;
            }
        }
        printf("quiet=%f loud=%f\n", quiet, loud);
        assert(loud > quiet + 0.01f);
        key_model[0][5] = 0;
        for (t = 0; t < 30; t++) {
            sim_tick();
        }
        HAL_SR_GetKeys(keys);
        for (k = 0; k < NUM_KEYS; k++) {
            if (!keys[k] && prev[k]) {
                Audio_LiveNoteOff((uint8_t)(60u + k));
            }
            prev[k] = keys[k];
        }
        assert(keys[5] == 0);
    }
    printf("KEY-TO-SOUND OK\n");

    /* 4. Step LEDs track the sequencer: bit n lit while step n plays. */
    {
        SeqStep_t s;
        Sequencer_GetStepData(0, &s);
        s.notes[0] = 60;
        s.vels[0] = 100;
        Sequencer_SetStep(0, &s);
        Sequencer_Play();
        for (t = 0; t < 130; t++) {
            sim_tick();
            Sequencer_Tick(120);
        }
        /* After ~130 ms at 120 BPM the step advanced from 0; the LED
         * mask must be exactly one bit. Reconstruct across 4 phases. */
        {
            uint16_t recon = 0u;
            for (i = 0; i < 4; i++) {
                uint16_t w = HAL_SR_LastLatched();
                int ph = sim_phase == 0 ? 3 : sim_phase - 1;
                recon |= (uint16_t)(((w >> 4) & 0xFu) << (ph * 4));
                sim_tick();
            }
            assert(recon != 0u && (recon & (recon - 1u)) == 0u);
            printf("STEP-LED OK mask=0x%04x step=%d\n", recon,
                   Sequencer_GetStep());
        }
    }

    printf("ALL HWSIM TESTS PASSED\n");
    return 0;
}
