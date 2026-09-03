/* target/stubs.c — DRY-RUN ONLY symbols for `make image-dryrun`.
 *
 * Never linked into the real firmware: the Dexed engine port provides
 * FM_*, board glue provides Uart0_SendByte, and JieLi libc provides
 * memset/memcpy at SDK integration time. These definitions exist solely
 * so the draft link script can be exercised (section placement, address
 * ranges) without the SDK libs. The mem* here are bounded byte loops with
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

void FM_Init(uint32_t sample_rate) { (void)sample_rate; }
void FM_NoteOn(uint8_t voice_id, uint8_t midi_note, uint8_t velocity)
{
    (void)voice_id; (void)midi_note; (void)velocity;
}
void FM_NoteOff(uint8_t voice_id) { (void)voice_id; }
void FM_Render(float *left_out, float *right_out, uint16_t num_frames)
{
    (void)left_out; (void)right_out; (void)num_frames;
}

void Uart0_SendByte(char c) { (void)c; }

/* Dry-run layout placeholder. The real board glue never returns
 * (watchdog reset into UBOOT OTA or retained-magic + system reset);
 * this no-op exists only so the draft link proves section placement. */
void OTA_JumpToBootloader(void) { }
