/* hal_shift_register.c
 *
 * Non-blocking 4x7 matrix scanner + 16-LED driver through 2x 74HC595D.
 *
 *  - Called from a 1 kHz timer ISR; NEVER blocks, NO heap, NO float.
 *  - Each ISR call services ONE row phase (0..3): shift 16 bits, latch,
 *    sample 7 column inputs, update per-key 3-sample debounce history.
 *  - Full matrix revisit = 4 ms; 3 consistent samples ~= 12 ms stability.
 *  - LEDs are double-buffered (led_shadow -> led_active) for tear-free ISR use.
 *
 * Future pi32v2 optimization: unroll the 16-iteration shift loop or drive the
 * shift via hardware SPI/DMA if the AC7911B8 pinmux allows it; the bit-bang
 * loop below is the deterministic fallback.
 */

#include "bsp_config.h"
#include "hal_shift_register.h"

/* ---------------------------------------------------------------------------
 * STATIC ALLOCATIONS ONLY — .bss, zero heap. Never malloc/calloc here.
 * ------------------------------------------------------------------------- */
static volatile uint16_t led_shadow = 0u; /* written by app/sequencer */
static volatile uint16_t led_active = 0u; /* latched copy consumed by ISR */

static volatile uint8_t key_stable[MATRIX_SLOTS];
static volatile uint8_t key_history[MATRIX_SLOTS]; /* low 3 bits = history */
static volatile uint8_t matrix_phase = 0u;         /* 0..MATRIX_ROWS-1 */

#ifdef UNIT_TEST_HOST
static volatile uint16_t last_latched = 0u;

uint16_t HAL_SR_LastLatched(void)
{
    return last_latched;
}
#endif

/* ---------------------------------------------------------------------------
 * Init
 * ------------------------------------------------------------------------- */
void HAL_SR_Init(void)
{
    /* Shift-register control pins = outputs (DIR bit = 0). */
    GPIOA_DIR &= (uint32_t)~SR_PINS_ALL;

    /* Column return pins = inputs (DIR bit = 1). */
    GPIOA_DIR |= KEY_COL_MASK;

    /* Idle bus state. */
    GPIOA_OUT &= (uint32_t)~(SR_DATA_PIN | SR_CLK_PIN | SR_LATCH_PIN);

    /* OE# active-low -> enable outputs. */
    GPIOA_OUT &= (uint32_t)~SR_OE_PIN;

    for (uint8_t i = 0u; i < MATRIX_SLOTS; i++) {
        key_stable[i]  = 0u;
        key_history[i] = 0u;
    }
    led_shadow   = 0u;
    led_active   = 0u;
    matrix_phase = 0u;
}

/* ---------------------------------------------------------------------------
 * 1 kHz ISR scan step
 * ------------------------------------------------------------------------- */
void HAL_SR_TimerISR(void)
{
    uint8_t phase = matrix_phase;

    /* 1. Build 16-bit payload for the cascaded 595 pair.
     *    [15:8] = duplicate LED nibble (deterministic data for 2nd chip),
     *    [7:4]  = LED nibble for this phase, [3:0] = one-hot row drive. */
    uint8_t  row_drive  = (uint8_t)(1u << phase);
    uint16_t led        = led_active;
    uint8_t  led_nibble = (uint8_t)((led >> (phase * LED_NIBBLE_PER_PHASE)) & 0x0Fu);
    uint16_t shift_word = (uint16_t)(((uint16_t)led_nibble << 12) |
                                    ((uint16_t)led_nibble << 4)  |
                                    (uint16_t)row_drive);

    /* 2. Bit-bang MSB-first. Rising edge of SRCK shifts one bit in. */
    for (int8_t i = 15; i >= 0; i--) {
        if (shift_word & (uint16_t)(1u << i)) {
            SR_DATA_HI();
        } else {
            SR_DATA_LO();
        }
        SR_CLK_HI();
        /* No busy-wait delay: 1 kHz ISR + GPIO setup time is sufficient on
         * the AC7911B8. Add 1-2 NOPs here only if scope shows setup violations.
         * TODO(pi32v2): __asm__ volatile("nop"); if needed. */
        SR_CLK_LO();
    }

    /* 3. Latch to output pins (RCK pulse). */
    SR_LATCH_HI();
    SR_LATCH_LO();
#ifdef UNIT_TEST_HOST
    last_latched = shift_word;
#endif

    /* 4. Sample columns for the row just driven. */
    uint32_t col_inputs =
        ((uint32_t)(GPIOA_IN & KEY_COL_MASK)) >> KEY_COL_SHIFT;

    /* 5. 3-sample shift-history debounce per column of this row. */
    for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
        uint8_t idx = (uint8_t)(phase * MATRIX_COLS + col);
        uint8_t cur = (col_inputs & (1u << col)) ? 1u : 0u;

        uint8_t h = (uint8_t)(((key_history[idx] << 1) | cur) & 0x07u);
        key_history[idx] = h;
        if (h == 0x07u) {
            key_stable[idx] = 1u;
        } else if (h == 0x00u) {
            key_stable[idx] = 0u;
        }
        /* else: keep previous stable state (still bouncing). */
    }

    /* 6. Advance phase. */
    matrix_phase = (uint8_t)((phase + 1u) % MATRIX_ROWS);
}

/* ---------------------------------------------------------------------------
 * Public accessors (called from main/sequencer context, not from this ISR)
 * ------------------------------------------------------------------------- */
void HAL_SR_SetLEDs(uint16_t mask)
{
    led_shadow = mask;
    /* Single 16-bit store is atomic on pi32v2 LE; guard anyway for portability. */
    unsigned s = IRQ_SAVE();
    led_active = led_shadow;
    IRQ_RESTORE(s);
}

void HAL_SR_GetKeys(uint8_t out[NUM_KEYS])
{
    unsigned s = IRQ_SAVE();
    for (uint8_t i = 0u; i < NUM_KEYS; i++) {
        out[i] = key_stable[i]; /* slot 27 (idx 27) is NC and never copied */
    }
    IRQ_RESTORE(s);
}
