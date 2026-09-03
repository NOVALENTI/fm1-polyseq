/* fm_env_test.cc — BIT-EXACT cross-check: original msfa Env (C++, from
 * tests/refcheck/msfa_orig/, verbatim upstream + stub Dexed.h) vs the C99
 * FmEnv port. Identical scripts must produce identical getsample streams.
 * Compile with a C++ driver; fm objects built as C (proves C linkage). */
#include <stdio.h>
#include <assert.h>

#include "msfa_orig/env.h"
#include "fm_env.h"

static const int kRates[4] = {80, 60, 40, 30};
static const int kLevels[4] = {99, 80, 60, 0};
static const int kRatesFlat[4] = {0, 0, 0, 0};
static const int kLevelsFlat[4] = {0, 0, 0, 0};

static void run_scripted(int use_flat)
{
    const int *r = use_flat ? kRatesFlat : kRates;
    const int *l = use_flat ? kLevelsFlat : kLevels;
    Env ref;
    FmEnv port;
    int32_t rs, ps;

    Env::init_sr(48000.0);
    FmEnv_InitSr(48000.0);
    ref.init(r, l, 3168, 0);
    FmEnv_Init(&port, r, l, 3168, 0);

    /* Attack/decay/sustain trajectory. */
    for (int i = 0; i < 4000; i++) {
        rs = ref.getsample();
        ps = FmEnv_GetSample(&port);
        if (rs != ps) {
            printf("MISMATCH at %d: ref=%d port=%d\n", i, rs, ps);
        }
        assert(rs == ps);
    }
    /* Release trajectory. */
    ref.keydown(false);
    FmEnv_KeyDown(&port, 0);
    for (int i = 0; i < 4000; i++) {
        rs = ref.getsample();
        ps = FmEnv_GetSample(&port);
        if (rs != ps) {
            printf("REL MISMATCH at %d: ref=%d port=%d\n", i, rs, ps);
        }
        assert(rs == ps);
    }
    assert(ref.isActive() == (FmEnv_IsActive(&port) != 0));
}

int main(void)
{
    /* Standard ADSR-ish + static-count (all-zero) trajectories. */
    run_scripted(0);
    run_scripted(1);

    /* transfer() mid-flight: fork at sample 500, trajectories must match. */
    {
        Env ref, ref2;
        FmEnv port, port2;
        Env::init_sr(48000.0);
        FmEnv_InitSr(48000.0);
        ref.init(kRates, kLevels, 3168, 0);
        FmEnv_Init(&port, kRates, kLevels, 3168, 0);
        for (int i = 0; i < 500; i++) {
            assert(ref.getsample() == FmEnv_GetSample(&port));
        }
        ref2.transfer(ref);
        FmEnv_Transfer(&port2, &port);
        for (int i = 0; i < 2000; i++) {
            assert(ref2.getsample() == FmEnv_GetSample(&port2));
        }
    }

    /* update() path matches. */
    {
        Env ref;
        FmEnv port;
        ref.init(kRates, kLevels, 3168, 0);
        FmEnv_Init(&port, kRates, kLevels, 3168, 0);
        ref.update(kRatesFlat, kLevelsFlat, 1000, 2);
        FmEnv_Update(&port, kRatesFlat, kLevelsFlat, 1000, 2);
        for (int i = 0; i < 1000; i++) {
            assert(ref.getsample() == FmEnv_GetSample(&port));
        }
    }

    printf("ALL FM ENV TESTS PASSED\n");
    return 0;
}
