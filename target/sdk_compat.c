/* target/sdk_compat.c — SDK header coexistence proof (compile-only).
 *
 * This TU includes our public firmware headers alongside the real JieLi
 * AC79 SDK headers. It proves, mechanically on every `make sdk-compat`
 * run, that our macro/function/type names coexist with the SDK (no
 * collisions) and that our headers parse under the SDK's own newlib
 * environment. It intentionally references a handful of SDK APIs
 * (gpio/dac/iis/timer/uart decls) so missing-header regressions fail
 * loudly here instead of at SDK integration time.
 *
 * SDK tree expected at build/sdk (gitignored scratch, see README flow);
 * JIELI_SDK ?= build/sdk/fw-AC79_AIoT_SDK or the extracted equivalent.
 * Not part of any firmware image. */

#include "bsp_config.h"
#include "hal_shift_register.h"
#include "sequencer.h"
#include "audio_core.h"
#include "fm_stub.h"
#include "fm_voice.h"
#include "ota_guard.h"
#include "ota_dispatch.h"
#include "bringup_probe.h"
#include "debug_midi.h"

/* A selection of SDK driver surfaces we integrate against. If any header
 * moves or any of our names collide, this TU stops compiling. */
#include "driver/cpu/wl82/asm/gpio.h"
#include "driver/cpu/wl82/asm/dac.h"
#include "driver/cpu/wl82/asm/iis.h"
#include "driver/device/gpio.h"
#include "driver/device/usb/device/usb_stack.h"
#include "driver/device/usb/device/uac_audio.h"
#include "system/timer.h"

/* Touch the key SDK APIs so the reference actually binds. JL_TIMER_TypeDef
 * and JL_UART_TypeDef live in asm/WL82.h (needs the SDK prefix build env),
 * so the timer hook binds the stable system/timer.h API instead. */
static void fm_sdk_touch(void)
{
    /* GPIO pin namespace + port struct used by bsp_config rationale. */
    volatile uint32_t sink;
    sink = (uint32_t)IO_PORTA_00;
    sink |= (uint32_t)sizeof(JL_PORT_FLASH_TypeDef);
    /* DAC/IIS open sequences referenced by audio_core's SDK HOOKUP. */
    sink |= (uint32_t)sizeof(struct dac_platform_data);
    sink |= (uint32_t)sizeof(struct iis_platform_data);
    /* USB MIDI-streaming subclass used by the OTA feed path recipe. */
    sink |= (uint32_t)USB_SUBCLASS_MIDISTREAMING;
    /* ms-granularity timer referenced by app_main integration notes. */
    (void)sys_timer_add;
    (void)sink;
    (void)HAL_SR_Init;
    (void)Sequencer_Tick;
    (void)Audio_Process_Callback;
    (void)FM_Init;
    (void)FM_LoadPatch;
    (void)OTA_Guard_FeedByte;
    (void)OTA_Dispatch_Poll;
    (void)Probe_SweepStep;
    (void)Debug_DumpPendingEvents;
}

void FmSdkCompat_Entry(void)
{
    fm_sdk_touch();
}
