/* Probe test: one-hot shift sweep reporting + LED walk logging.
 * A scripted column map stands in for the physical matrix. */
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef UNIT_TEST_HOST
#define UNIT_TEST_HOST
#endif
#include "bsp_config.h"

volatile uint32_t HOST_GPIOA_DIR, HOST_GPIOA_OUT, HOST_GPIOA_IN;

#include "hal_shift_register.h"
#include "bringup_probe.h"

static char logbuf[256];
static unsigned logpos = 0;
static void cap_putc(char c)
{
    if (logpos < sizeof(logbuf) - 1u) {
        logbuf[logpos++] = c;
    }
}
static void log_reset(void)
{
    logpos = 0;
    logbuf[0] = '\0';
}
static const char *log_str(void)
{
    logbuf[logpos] = '\0';
    return logbuf;
}

int main(void)
{
    HAL_SR_Init();

    /* Fixed column field 0x15 on probed bit 5. */
    HOST_GPIOA_IN = (uint32_t)(0x15u << KEY_COL_SHIFT);
    log_reset();
    Probe_ShiftOne(5, cap_putc);
    assert(strcmp(log_str(), "P05 IN=15\n") == 0);

    /* All columns released on bit 0. */
    HOST_GPIOA_IN = 0u;
    log_reset();
    Probe_ShiftOne(0, cap_putc);
    assert(strcmp(log_str(), "P00 IN=00\n") == 0);

    /* Out-of-range bit and null sink: silent, no crash. */
    log_reset();
    Probe_ShiftOne(16, cap_putc);
    Probe_ShiftOne(3, 0);
    assert(logpos == 0u);

    /* LED walk logs the staged step bit. */
    log_reset();
    Probe_LEDOne(7, cap_putc);
    assert(strcmp(log_str(), "LED07\n") == 0);
    log_reset();
    Probe_LEDOne(16, cap_putc);
    assert(logpos == 0u);

    printf("ALL PROBE TESTS PASSED\n");
    return 0;
}
