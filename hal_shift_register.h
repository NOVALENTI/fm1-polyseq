#ifndef HAL_SHIFT_REGISTER_H
#define HAL_SHIFT_REGISTER_H

#include <stdint.h>
#include "bsp_config.h"

/* Init shift-register GPIOs and clear scanner state. Call once at boot. */
void HAL_SR_Init(void);

/* Non-blocking scan step. Call from 1 kHz timer ISR (Timer 2/3) ONLY.
 * Advances one row phase per call: shift-out -> latch -> sample -> debounce. */
void HAL_SR_TimerISR(void);

/* Stage a 16-bit LED mask (bit n = LED n). ISR picks it up tear-free. */
void HAL_SR_SetLEDs(uint16_t mask);

/* Copy debounced key states (0/1 per key, 27 entries) for app use. */
void HAL_SR_GetKeys(uint8_t out[NUM_KEYS]);

#endif /* HAL_SHIFT_REGISTER_H */
