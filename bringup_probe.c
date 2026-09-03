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

static void put2hex(Debug_PutcFn f, uint8_t v)
{
    uint8_t hi = (uint8_t)((v >> 4) & 0x0Fu);
    uint8_t lo = (uint8_t)(v & 0x0Fu);
    f((char)(hi < 10u ? ('0' + hi) : ('A' + hi - 10u)));
    f((char)(lo < 10u ? ('0' + lo) : ('A' + lo - 10u)));
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
