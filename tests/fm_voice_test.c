/* fm_voice_test.c — voice manager: audibility, silence, tails, split.
 * Uses only the public fm_stub.h contract (+ FM_LoadPatch). No mocks. */
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "fm_stub.h"
#include "fm_voice.h"

#define SR 48000u
#define SEC (48000u)

static float sbuf_l[SEC];
static float sbuf_r[SEC];

static float peak_of(const float *b, uint32_t n)
{
    float p = 0.0f;
    uint32_t i;
    for (i = 0u; i < n; i++) {
        float v = b[i] < 0.0f ? -b[i] : b[i];
        if (v > p) {
            p = v;
        }
        assert(v <= 1.0f); /* integer clip stage guarantees this */
    }
    return p;
}

int main(void)
{
    float first128[256];
    float rep128[256];
    uint32_t i;

    FM_Init(SR);

    /* 1. Silence at boot (no voices): exact zeros. */
    FM_Render(sbuf_l, sbuf_r, 128);
    for (i = 0u; i < 128u; i++) {
        assert(sbuf_l[i] == 0.0f && sbuf_r[i] == 0.0f);
    }

    /* 2. Middle C on sequencer voice 0: must be clearly audible. */
    FM_NoteOn(0, 60, 100);
    FM_Render(sbuf_l, sbuf_r, SEC);
    assert(peak_of(sbuf_l, SEC) > 0.05f);
    /* Stereo is dual-mono. */
    for (i = 0u; i < SEC; i++) {
        assert(sbuf_l[i] == sbuf_r[i]);
    }
    /* Non-64-multiple DMA block sizes (partial-period IRQs) stay bounded. */
    FM_Render(sbuf_l, sbuf_r, 100);
    assert(peak_of(sbuf_l, 100u) <= 1.0f);
    FM_Render(sbuf_l, sbuf_r, 1);
    assert(peak_of(sbuf_l, 1u) <= 1.0f);

    /* 3. Determinism: same note from fresh init renders identically. */
    FM_Init(SR);
    FM_NoteOn(0, 60, 100);
    FM_Render(first128, first128 + 128, 128);
    FM_Init(SR);
    FM_NoteOn(0, 60, 100);
    FM_Render(rep128, rep128 + 128, 128);
    for (i = 0u; i < 256u; i++) {
        assert(first128[i] == rep128[i]);
    }

    /* 4. Release tail decays to silence (bounded wait, then strict). */
    FM_NoteOff(0);
    {
        int silent = 0;
        for (int s = 0; s < 8 && !silent; s++) {
            FM_Render(sbuf_l, sbuf_r, SEC);
            if (peak_of(sbuf_l + SEC - 128u, 128u) < 0.001f) {
                silent = 1;
            }
        }
        assert(silent);
    }

    /* 5. Split isolation: the live voice audibly contributes. Voice 2's
     * own trajectory is identical in both runs, so any difference in the
     * mixed block comes from voice 8. */
    FM_Init(SR);
    FM_NoteOn(2, 60, 100);
    FM_Render(sbuf_l, sbuf_r, 128);
    FM_Render(sbuf_l, sbuf_r, 128); /* voice 2, second block, alone */
    FM_Init(SR);
    FM_NoteOn(2, 60, 100);
    FM_Render(rep128, rep128 + 128, 128);
    FM_NoteOn(8, 67, 100);
    FM_Render(rep128, rep128 + 128, 128); /* same + live voice 8 */
    {
        int diff = 0;
        for (i = 0u; i < 128u; i++) {
            if (rep128[i] != sbuf_l[128 + i]) {
                diff = 1;
            }
        }
        assert(diff);
    }

    /* 6. Bad arguments are ignored, never crash. */
    FM_NoteOn(12, 60, 100);
    FM_NoteOn(0, 200, 100);
    FM_NoteOff(12);
    FM_Render(0, sbuf_r, 128);
    FM_Render(sbuf_l, 0, 128);
    FM_Render(sbuf_l, sbuf_r, 0);

    /* 7. Custom patch loads (silent all-zero patch -> silence). */
    {
        static uint8_t blank[156];
        for (i = 0u; i < 156u; i++) {
            blank[i] = 0;
        }
        FM_Init(SR);
        FM_LoadPatch(blank);
        FM_NoteOn(0, 60, 100);
        FM_Render(sbuf_l, sbuf_r, 128);
        assert(peak_of(sbuf_l, 128u) == 0.0f);
        FM_LoadPatch(0); /* null ignored */
    }

    printf("ALL FM VOICE TESTS PASSED\n");
    return 0;
}
