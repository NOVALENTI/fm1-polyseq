/* app_main.c — reference integration (JieLi AC7911B8).
 *
 * Wiring:
 *   1 kHz Timer 2/3 ISR -> HAL_SR_TimerISR() + Sequencer_Tick(bpm)
 *   Main loop           -> poll HAL_SR_GetKeys(), edge-detect, live voices 6..11
 *   I2S DMA half/full   -> Audio_Process_Callback() into CS4344
 *
 * ISR partitioning: no float/printf in the timer ISR; all heap-free.
 * Replace Timer/I2S registration stubs with the JieLi SDK calls.
 */
#include <stdint.h>
#include "bsp_config.h"
#include "hal_shift_register.h"
#include "sequencer.h"
#include "audio_core.h"

static uint32_t g_bpm = 120u;
static uint8_t prev_keys[NUM_KEYS];
static uint8_t cur_keys[NUM_KEYS];

/* Call this from the 1 kHz hardware timer ISR (Timer 2/3). */
void Timer2_1kHz_ISR(void)
{
    HAL_SR_TimerISR();
    Sequencer_Tick(g_bpm);
}

/* Call this from I2S DMA half/full-complete IRQ. */
void I2S_DMA_IRQ(float *dma_buf, uint16_t frames)
{
    Audio_Process_Callback(dma_buf, frames);
}

int main(void)
{
    uint8_t i;

    HAL_SR_Init();
    Sequencer_Init();
    Audio_Init();
    for (i = 0u; i < NUM_KEYS; i++) {
        prev_keys[i] = 0u;
    }
    Sequencer_Play();

    /* TODO(target): register Timer2_1kHz_ISR at SCAN_TICK_HZ and start I2S
     * DMA at SAMPLE_RATE/BLOCK_FRAMES here. */

    for (;;) {
        HAL_SR_GetKeys(cur_keys);
        for (i = 0u; i < NUM_KEYS; i++) {
            if (cur_keys[i] && !prev_keys[i]) {
                /* Key press -> chromatic live note, velocity 100. */
                Audio_LiveNoteOn((uint8_t)(60u + i), 100u);
            } else if (!cur_keys[i] && prev_keys[i]) {
                Audio_LiveNoteOff((uint8_t)(60u + i));
            }
            prev_keys[i] = cur_keys[i];
        }
        /* TODO(target): WFI / RTOS delay; sequencer + audio run in ISRs. */
    }
    /* return 0; */
}
