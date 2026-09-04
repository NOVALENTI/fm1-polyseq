/* target/stubs.c — DRY-RUN ONLY symbols for `make image-dryrun`.
 *
 * Never linked into the real firmware: the FM_* engine is real code now
 * (fm/fm_voice.c), board glue provides Uart0_SendByte, JieLi libc provides
 * memset/memcpy, and newlib/compiler-rt provides the boot-time float
 * helpers at SDK integration time. The definitions below exist solely so
 * the draft link script can be exercised (section placement, address
 * ranges) without the SDK libs. The mem* are bounded byte loops with
 * identical semantics to libc; compiled -fno-builtin so they cannot
 * self-recurse. */
#include <stdint.h>

void *memset(void *dst, int c, unsigned long n)
{
    uint8_t *p = (uint8_t *)dst;
    while (n--) {
        *p++ = (uint8_t)c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void Uart0_SendByte(char c) { (void)c; }

/* SDK-provided platform symbols (real definitions come from the SDK
 * link: UPDATA_BEG is linker-reserved retained RAM, system_reset
 * reboots into UBOOT). Placeholders exist only for layout proof. */
uint32_t UPDATA_BEG[32];
void system_reset(void) { }
