/* bringup_probe.c — see bringup_probe.h.
 *
 * Bit-bang order matches hal_shift_register.c (MSB-first, latch pulse),
 * duplicated here so the probe works even if the scanner ISR is stopped.
 * Output format is fixed-width for easy diffing of sweep logs. */

#include "bringup_probe.h"
#include "hal_shift_register.h"

static void put2dec(Debug_PutcFn f, uint8_t v)
{
    f((char)('0' + (v / 10u)));
    f((char)('0' + (v % 10u)));
}

static char hex_digit(uint8_t v)
{
    uint8_t d = (uint8_t)(v & 0x0Fu);
    /* Both ?: operands are uint8_t: no sign-compare warning on GCC/Clang. */
    return (char)(d + ((d < 10u) ? (uint8_t)'0' : (uint8_t)('A' - 10)));
}

static void put2hex(Debug_PutcFn f, uint8_t v)
{
    f(hex_digit((uint8_t)(v >> 4)));
    f(hex_digit(v));
}

static void shift_word_out(uint16_t w)
{
    int8_t i;
    for (i = 15; i >= 0; i--) {
        if (w & (uint16_t)(1u << i)) {
            SR_DATA_HI();
        } else {
            SR_DATA_LO();
        }
        SR_CLK_HI();
        /* TODO(pi32v2): __asm__ volatile("nop"); if scope shows violations. */
        SR_CLK_LO();
    }
    SR_LATCH_HI();
    SR_LATCH_LO();
}

void Probe_ShiftOne(uint8_t bit, Debug_PutcFn putc_fn)
{
    uint8_t cols;
    if (putc_fn == 0 || bit > 15u) {
        return;
    }
    shift_word_out((uint16_t)(1u << bit));
    cols = (uint8_t)(((uint32_t)(GPIOA_IN & KEY_COL_MASK)) >> KEY_COL_SHIFT);
    putc_fn('P');
    put2dec(putc_fn, bit);
    putc_fn(' ');
    putc_fn('I');
    putc_fn('N');
    putc_fn('=');
    put2hex(putc_fn, (uint8_t)(cols & 0x7Fu));
    putc_fn('\n');
}

void Probe_LEDOne(uint8_t led, Debug_PutcFn putc_fn)
{
    if (putc_fn == 0 || led > 15u) {
        return;
    }
    HAL_SR_SetLEDs((uint16_t)(1u << led));
    putc_fn('L');
    putc_fn('E');
    putc_fn('D');
    put2dec(putc_fn, led);
    putc_fn('\n');
}

#define PROBE_SWEEP_STEPS 32u

void Probe_SweepStep(Debug_PutcFn putc_fn)
{
    static uint8_t pos = 0u;
    if (putc_fn == 0) {
        return;
    }
    if (pos < 16u) {
        Probe_ShiftOne(pos, putc_fn);
    } else {
        Probe_LEDOne((uint8_t)(pos - 16u), putc_fn);
    }
    pos = (uint8_t)((pos + 1u) % PROBE_SWEEP_STEPS);
}

void Probe_SweepOnce(Debug_PutcFn putc_fn)
{
    uint8_t i;
    for (i = 0u; i < PROBE_SWEEP_STEPS; i++) {
        Probe_SweepStep(putc_fn);
    }
}
