#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#include <stdint.h>

/* --------------------------------------------------------------------------
 * HYPOTHETICAL MEMORY-MAPPED GPIO REGISTERS (JieLi AC7911B8)
 * Replace with exact pi32v2 MMIO addresses when reverse-engineered.
 * All pin mapping changes belong HERE ONLY.
 * -------------------------------------------------------------------------- */
#ifdef UNIT_TEST_HOST
/* Host-simulated registers for logic testing (NOT on target). */
extern volatile uint32_t HOST_GPIOA_DIR;
extern volatile uint32_t HOST_GPIOA_OUT;
extern volatile uint32_t HOST_GPIOA_IN;
#define GPIOA_DIR  (HOST_GPIOA_DIR)
#define GPIOA_OUT  (HOST_GPIOA_OUT)
#define GPIOA_IN   (HOST_GPIOA_IN)
#else
#define GPIOA_BASE   0x40001000u
#define GPIOA_DIR    (*(volatile uint32_t *)(GPIOA_BASE + 0x00u)) /* 1=In,0=Out */
#define GPIOA_OUT    (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_IN     (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#endif

/* --------------------------------------------------------------------------
 * SHIFT REGISTER (74HC595D x2, cascaded 16-bit) PIN MASKS — Port A
 * -------------------------------------------------------------------------- */
#define SR_DATA_PIN  (1u << 0)  /* PA0 */
#define SR_CLK_PIN   (1u << 1)  /* PA1 : SRCK  */
#define SR_LATCH_PIN (1u << 2)  /* PA2 : RCK   */
#define SR_OE_PIN    (1u << 3)  /* PA3 : OE# active-low */

#define SR_PINS_ALL  (SR_DATA_PIN | SR_CLK_PIN | SR_LATCH_PIN | SR_OE_PIN)

/* --------------------------------------------------------------------------
 * KEY MATRIX: 4 Rows x 7 Cols = 28 slots, 27 physical keys (slot 27 = NC)
 * Rows are DRIVEN by low 4 bits of the 595 chain; columns READ on PA4..PA10.
 * LEDs: 16 total, 4 per scan phase (nibble of led_active selected by phase).
 * -------------------------------------------------------------------------- */
#define MATRIX_ROWS  4
#define MATRIX_COLS  7
#define NUM_KEYS     27
#define MATRIX_SLOTS (MATRIX_ROWS * MATRIX_COLS) /* 28 */
#define NUM_LEDS     16

/* Columns on PA4..PA10 */
#define KEY_COL_SHIFT 4
#define KEY_COL_MASK  (0x7Fu << KEY_COL_SHIFT)

/* 16-bit shift word layout (MSB first on wire):
 *   bits[15:8] = LED extension / second 595 (upper nibble per phase + spare)
 *   bits[7:4]  = LED nibble for current phase
 *   bits[3:0]  = one-hot row drive
 * Upper byte currently carries a copy of the LED nibble so both 595s get
 * deterministic data even if the PCB ties LEDs across both chips. */
#define LED_NIBBLE_PER_PHASE 4

/* --------------------------------------------------------------------------
 * SYSTEM TIMING & AUDIO DEFAULTS
 * Timer0/1 reserved for BT/RTOS -> sequencer/scan use Timer 2/3 at 1 kHz.
 * -------------------------------------------------------------------------- */
#define SEQ_TIMER_ID   2
#define SCAN_TICK_HZ   1000u   /* 1 ms matrix scan ISR */
#define SEQ_TICK_HZ    1000u   /* Sequencer_Tick rate; dt_us = 1000 */
#define SEQ_DT_US      (1000000u / SEQ_TICK_HZ)

#define SAMPLE_RATE    48000u
#define BLOCK_FRAMES   128u    /* ~2.66 ms per DMA block @48 kHz */
#define I2S_BITS       16

/* --------------------------------------------------------------------------
 * FAST BIT-BANG MACROS
 * -------------------------------------------------------------------------- */
#define SR_CLK_HI()   (GPIOA_OUT |= SR_CLK_PIN)
#define SR_CLK_LO()   (GPIOA_OUT &= (uint32_t)~SR_CLK_PIN)
#define SR_LATCH_HI() (GPIOA_OUT |= SR_LATCH_PIN)
#define SR_LATCH_LO() (GPIOA_OUT &= (uint32_t)~SR_LATCH_PIN)
#define SR_DATA_HI()  (GPIOA_OUT |= SR_DATA_PIN)
#define SR_DATA_LO()  (GPIOA_OUT &= (uint32_t)~SR_DATA_PIN)

/* Critical-section hooks: map to pi32v2 IRQ mask on target. */
#ifdef UNIT_TEST_HOST
#define IRQ_SAVE()    (0u)
#define IRQ_RESTORE(x) ((void)(x))
#else
/* TODO(pi32v2): replace with real global-interrupt disable/enable intrinsics,
 * e.g.  __asm__ volatile("cli" : "=r"(primask)); / ("sti"). */
#define IRQ_SAVE()    (0u)
#define IRQ_RESTORE(x) ((void)(x))
#endif

#endif /* BSP_CONFIG_H */
