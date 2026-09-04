/* app_main.c — reference integration (JieLi AC7911B8).
 *
 * Normal firmware (default):
 *   1 kHz Timer 2/3 ISR -> HAL_SR_TimerISR() + Sequencer_Tick(bpm)
 *   Main loop           -> poll HAL_SR_GetKeys(), edge-detect, live voices 6..11
 *   DAC DMA callback    -> Audio_Process_Callback() (internal DAC path)
 *
 * Bring-up probe firmware (-DBRINGUP_PROBE):
 *   Flash this INSTEAD of the sequencer to map the front panel over UART
 *   before the pinout is known. The main loop paces Probe_SweepStep over
 *   the 32-sub-step sweep (16 shift patterns + 16 LEDs). Sequencer, FM
 *   engine, and DAC are never started. Provide Uart0_SendByte() for target
 *   (host builds use putchar).
 *
 * ISR partitioning: no float/printf in the timer ISR; all heap-free.
 * Replace Timer/DAC registration stubs with the JieLi SDK calls.
 *
 * FLASH SAFETY (verified: aroum/fm1-custom-fw research + community):
 * The FM-1 PCB has NO JTAG/SWD/UART pads and NO recovery buttons; USB
 * MIDI SysEx is the ONLY update channel (header F0 00 32 45, cmds
 * 0x01 = verify/meta, 0x02 = start upgrade, 0x03 = data chunk 7-bit
 * encoded, 0x04 = preset, 0x58 = ACK; stock tool: M-UPGRADE-FM1,
 * container @JMUA/JLUFW). Consequence: DO NOT flash ANY build from this
 * repo (probe included) until (a) received USB-MIDI bytes reach
 * OTA_Guard_FeedByte and (b) the OTA jump path is confirmed on hardware
 * (ota_jump.c implements the official SDK handoff: UPDATA_PARM with CRC
 * + magic to UPDATA_FLAG_ADDR, then system_reset; type USB_HID_UPDATA
 * and ota_addr are the confirmable choices) — otherwise the device is
 * permanently soft-locked against future updates and stock rollback.
 * Detection (ota_guard) and quiesce-and-jump dispatch (ota_dispatch,
 * polled in both main loops below) already exist; the USB feed and
 * hardware confirmation are what remain.
 */
#include <stdint.h>
#include "bsp_config.h"
#include "hal_shift_register.h"
#include "sequencer.h"
#include "audio_core.h"
#include "ota_guard.h"
#include "ota_dispatch.h"

#ifdef BRINGUP_PROBE
#include "bringup_probe.h"
#ifdef UNIT_TEST_HOST
#include <stdio.h>
static void probe_putc(char c) { putchar(c); }
#else
/* Uart0_SendByte: polled TX recipe from the SDK (cpu/wl82/debug.c
 * uart_putchar), JL_UART0_BASE = 0x12000 (lsfr 0x10000 + map_adr(0x20,0)):
 *   JL_UART0->CON0 |= BIT(13); JL_UART0->BUF = c; csync;
 *   while (!(JL_UART0->CON0 & BIT(15))) {}
 * Implement in board glue once the probed UART port/baud is known. */
extern void Uart0_SendByte(char c); /* user-provided JieLi UART0 TX byte sink */
static void probe_putc(char c) { Uart0_SendByte(c); }
#endif
#endif

#ifndef BRINGUP_PROBE
static uint32_t g_bpm = 120u;
static uint8_t prev_keys[NUM_KEYS];
static uint8_t cur_keys[NUM_KEYS];
#endif

#ifndef BRINGUP_PROBE
/* Call this from the 1 kHz hardware timer ISR (Timer 2/3). */
void Timer2_1kHz_ISR(void)
{
    HAL_SR_TimerISR();
    Sequencer_Tick(g_bpm);
}

/* Call this from the DAC DMA data-handler (SDK dac_set_data_handler). */
void DAC_DMA_IRQ(float *dma_buf, uint16_t frames)
{
    Audio_Process_Callback(dma_buf, frames);
}
#endif

/* OTA hooks, one definition per firmware image (see ota_dispatch.h). */
#ifdef BRINGUP_PROBE
void OTA_QuenchAudio(void)
{
    /* Probe image runs no sound engine: nothing to silence. */
}
#else
void OTA_QuenchAudio(void)
{
    Sequencer_Stop(); /* immediate AllNotesOff for sequencer voices */
}
#endif

int main(void)
{
    HAL_SR_Init();
#ifdef BRINGUP_PROBE
    /* Probe build: no sequencer, no FM, no DAC. Pace one sweep sub-step
     * per loop iteration; insert ~50-100 ms delay per step on target.
     * OTA dispatch polled every iteration: the probe image must ALSO
     * retain bootloader re-entry (feed USB bytes via OTA_Guard_FeedByte
     * from the USB ISR once the MIDI stack exists). */
    OTA_Guard_Init();
    OTA_Dispatch_Init();
    for (;;) {
        Probe_SweepStep(probe_putc);
        OTA_Dispatch_Poll(); /* jumps to OTA on SysEx 0x01/0x02 */
        /* TODO(target): ~50-100 ms pace (timer tick or delay loop). */
    }
#else
    {
    uint8_t i;
    Sequencer_Init();
    Audio_Init();
    OTA_Guard_Init();
    OTA_Dispatch_Init();
    for (i = 0u; i < NUM_KEYS; i++) {
        prev_keys[i] = 0u;
    }
    Sequencer_Play();

    /* TODO(target): register Timer2_1kHz_ISR at SCAN_TICK_HZ and start the
     * DAC data handler at SAMPLE_RATE/BLOCK_FRAMES here. SDK hooks (all verified in
     * fw-AC79_AIoT_SDK): JL_TIMER2 at 0x10600 (CON/CNT/PRD/PWM) for the
     * 1 kHz tick, or sys_timer_add(priv, func, msec) from system/timer.h
     * for ms-granularity callbacks. Audio out: dac_open() +
     * dac_set_sample_rate(48000) + dac_set_data_handler(buf fill) +
     * dac_on() (asm/dac.h, sr_points = BLOCK_FRAMES); external DACs via
     * iis_open(&pd, idx) + iis_channel_on() (asm/iis.h). XIP NOR flash rom
     * ORIGIN is 0x2000120 with internal ram0 at 0x1c00000
     * (cpu/wl82/sdk_ld_sfc.c, NO_SDRAM). */

    for (;;) {
        HAL_SR_GetKeys(cur_keys);
        OTA_Dispatch_Poll(); /* SysEx OTA re-entry (see FLASH SAFETY) */
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
    }
#endif
    /* return 0; */
}
