/* fm_kernel_test.cc — BIT-EXACT cross-check: original msfa FmOpKernel
 * (C++, vendored in tests/refcheck/msfa_orig/) vs the C99 port, over the
 * shared sine table path (both tables initialized from identical code).
 * Compares all 64 outputs per call plus feedback state carry-over. */
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "msfa_orig/fm_op_kernel.h"
#include "msfa_orig/sin.h"
#include "fm_op_kernel.h"
#include "fm_sin.h"
#include "fm_common.h"

static int32_t ref_in[FM_N];
static int32_t ref_out[FM_N];
static int32_t port_in[FM_N];
static int32_t port_out[FM_N];

/* Deterministic PRNG (xorshift32) so configs are reproducible. */
static uint32_t rng_state = 0x12345678u;
static uint32_t next_rand(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void fill_inputs(void)
{
    for (int i = 0; i < FM_N; i++) {
        int32_t v = (int32_t)(next_rand() & 0xffffffu) - (1 << 23);
        ref_in[i] = v;
        port_in[i] = v;
        ref_out[i] = (int32_t)(next_rand() & 0xffu);
        port_out[i] = ref_out[i];
    }
}

int main(void)
{
    Sin::init();
    FmSin_Init();

    /* compute / compute_pure x {overwrite, accumulate} x random configs. */
    for (int cfg = 0; cfg < 40; cfg++) {
        int32_t phase0 = (int32_t)next_rand();
        int32_t freq = (int32_t)(next_rand() & 0xffffffu) - (1 << 20);
        int32_t gain1 = (int32_t)(next_rand() & 0xffffffu);
        int32_t gain2 = (int32_t)(next_rand() & 0xffffffu);
        int add = cfg & 1;
        fill_inputs();

        FmOpKernel::compute(ref_out, ref_in, phase0, freq, gain1, gain2,
                            add != 0);
        FmOpKernel_Compute(port_out, port_in, phase0, freq, gain1, gain2,
                           add);
        assert(memcmp(ref_out, port_out, sizeof(ref_out)) == 0);

        fill_inputs();
        FmOpKernel::compute_pure(ref_out, phase0, freq, gain1, gain2,
                                 add != 0);
        FmOpKernel_ComputePure(port_out, phase0, freq, gain1, gain2, add);
        assert(memcmp(ref_out, port_out, sizeof(ref_out)) == 0);
    }

    /* compute_fb incl. multi-block state carry and fb_shift sweep. */
    for (int shift = 0; shift < 8; shift++) {
        int32_t ref_fb[2] = {(int32_t)next_rand(), (int32_t)next_rand()};
        int32_t port_fb[2] = {ref_fb[0], ref_fb[1]};
        for (int blk = 0; blk < 4; blk++) {
            int32_t phase0 = (int32_t)next_rand();
            int32_t freq = (int32_t)(next_rand() & 0xffffffu);
            int32_t gain1 = (int32_t)(next_rand() & 0xffffffu);
            int32_t gain2 = (int32_t)(next_rand() & 0xffffffu);
            int add = blk & 1;
            fill_inputs();
            FmOpKernel::compute_fb(ref_out, phase0, freq, gain1, gain2,
                                   ref_fb, shift, add != 0);
            FmOpKernel_ComputeFb(port_out, phase0, freq, gain1, gain2,
                                 port_fb, shift, add);
            assert(memcmp(ref_out, port_out, sizeof(ref_out)) == 0);
            assert(ref_fb[0] == port_fb[0] && ref_fb[1] == port_fb[1]);
        }
    }

    printf("ALL FM KERNEL TESTS PASSED\n");
    return 0;
}
