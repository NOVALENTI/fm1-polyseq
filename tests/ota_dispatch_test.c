/* Dispatch tests: request -> quench (sequencer stopped, offs queued) -> jump once. */
#include <stdio.h>
#include <assert.h>

#ifndef UNIT_TEST_HOST
#define UNIT_TEST_HOST
#endif
#include "bsp_config.h"

volatile uint32_t HOST_GPIOA_DIR, HOST_GPIOA_OUT, HOST_GPIOA_IN;

#include "hal_shift_register.h"
#include "sequencer.h"
#include "ota_guard.h"
#include "ota_dispatch.h"

static int jump_fired = 0;
static int quench_fired = 0;

/* Board hook mock: records the call and returns (real one never returns). */
void OTA_JumpToBootloader(void)
{
    jump_fired++;
}

/* App hook mock: records, then performs the full-image behavior so the
 * test proves offs actually get queued on quench. */
void OTA_QuenchAudio(void)
{
    quench_fired++;
    Sequencer_Stop();
}

static const uint8_t kUpgrade[] = {0xF0, 0x00, 0x32, 0x45, 0x02, 0xF7};
static const uint8_t kVerify[] = {0xF0, 0x00, 0x32, 0x45, 0x01, 0xF7};

static void hold_a_chord(void)
{
    SeqStep_t s;
    Sequencer_GetStepData(0, &s);
    s.notes[0] = 60;
    s.vels[0] = 100;
    s.gate_pct = 80;
    s.swing_us = 0;
    s.active = 1;
    Sequencer_SetStep(0, &s);
    Sequencer_Play();
    for (int t = 0; t < 130; t++) {
        Sequencer_Tick(120); /* cross step boundary: voices now held */
    }
}

int main(void)
{
    HAL_SR_Init();

    /* 1. Upgrade request: quench starts, sequencer stops, offs queued. */
    Sequencer_Init();
    OTA_Guard_Init();
    OTA_Dispatch_Init();
    hold_a_chord();
    OTA_Guard_Feed(kUpgrade, (uint16_t)sizeof(kUpgrade));
    OTA_Dispatch_Poll();
    assert(OTA_Dispatch_State() == OTA_DS_QUENCH);
    assert(quench_fired == 1);
    assert(Sequencer_IsPlaying() == 0u);
    assert(SEQ_FIFO_Pending() > 0u); /* AllNotesOff queued for held voices */

    /* 2. Jump fires exactly on the 9th poll, never again. */
    for (int k = 0; k < 7; k++) {
        OTA_Dispatch_Poll();
        assert(jump_fired == 0);
    }
    OTA_Dispatch_Poll(); /* quench reaches OTA_QUENCH_POLLS */
    assert(jump_fired == 1);
    assert(OTA_Dispatch_State() == OTA_DS_DONE);
    for (int k = 0; k < 10; k++) {
        OTA_Dispatch_Poll();
    }
    assert(jump_fired == 1);

    /* 3. Verify-only request also triggers the handoff. */
    Sequencer_Init();
    OTA_Guard_Init();
    OTA_Dispatch_Init();
    jump_fired = 0;
    hold_a_chord();
    OTA_Guard_Feed(kVerify, (uint16_t)sizeof(kVerify));
    for (int k = 0; k < 10; k++) {
        OTA_Dispatch_Poll();
    }
    assert(jump_fired == 1);

    /* 4. No request: polls stay idle, no jump. */
    OTA_Guard_Init();
    OTA_Dispatch_Init();
    jump_fired = 0;
    for (int k = 0; k < 20; k++) {
        OTA_Dispatch_Poll();
    }
    assert(jump_fired == 0);
    assert(OTA_Dispatch_State() == OTA_DS_IDLE);

    printf("ALL DISPATCH TESTS PASSED\n");
    return 0;
}
