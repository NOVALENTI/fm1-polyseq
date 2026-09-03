/* Host logic test: HAL debounce + sequencer tick + audio bridge.
 * Build: make host. Mocks the FM engine and GPIO registers. */
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

/* Mock FM engine: sequencer must never address live voices 6..11. */
static int on_cnt = 0, off_cnt = 0, render_cnt = 0;
void FM_Init(uint32_t sr) { (void)sr; }
void FM_NoteOn(uint8_t v, uint8_t n, uint8_t vel)
{
    assert(v < FM_SEQ_VOICES);
    (void)n; (void)vel; on_cnt++;
}
void FM_NoteOff(uint8_t v) { assert(v < FM_SEQ_VOICES); off_cnt++; }
void FM_Render(float *l, float *r, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++) { l[i] = 0.1f; r[i] = 0.1f; }
    render_cnt++;
}

int main(void)
{
    HAL_SR_Init();
    uint8_t keys[NUM_KEYS];
    for (int t = 0; t < 20; t++) {
        HOST_GPIOA_IN = (1u << (KEY_COL_SHIFT + 0));
        HAL_SR_TimerISR();
    }
    HAL_SR_GetKeys(keys);
    assert(keys[0] == 1);
    for (int t = 0; t < 20; t++) { HOST_GPIOA_IN = 0; HAL_SR_TimerISR(); }
    HAL_SR_GetKeys(keys);
    assert(keys[0] == 0);
    printf("HAL debounce OK\n");

    /* Boot alignment: audio + sequencer clocks both start at 0. */
    Sequencer_Init();
    Audio_Init();
    SeqStep_t s;
    Sequencer_GetStepData(0, &s);
    s.notes[0] = 60; s.vels[0] = 100;
    s.notes[1] = 64; s.vels[1] = 90;
    s.gate_pct = 50; s.swing_us = 0; s.active = 1;
    Sequencer_SetStep(0, &s);
    for (int i = 1; i < 16; i++) {
        SeqStep_t m = s; m.active = 0; Sequencer_SetStep((uint8_t)i, &m);
    }
    Sequencer_Play();

    float buf[2 * BLOCK_FRAMES];
    int audio_calls = 0;
    for (int t = 0; t < 400; t++) {
        Sequencer_Tick(120);
        if ((t % 3) == 2) {
            Audio_Process_Callback(buf, BLOCK_FRAMES);
            audio_calls++;
        }
    }
    for (int k = 0; k < 60; k++) {
        Sequencer_Tick(120);
        Audio_Process_Callback(buf, BLOCK_FRAMES);
        audio_calls++;
    }
    printf("SEQ/AUDIO pending=%d on=%d off=%d blocks=%d seq_now=%lu\n",
           SEQ_FIFO_Pending(), on_cnt, off_cnt, audio_calls,
           (unsigned long)Sequencer_NowUs());
    assert(on_cnt == 2);
    assert(off_cnt >= 2);
    assert(buf[0] > 0.09f && buf[0] <= 1.0f);
    printf("ALL HOST TESTS PASSED\n");
    return 0;
}
