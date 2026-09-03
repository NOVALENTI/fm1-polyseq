/* fm_core_test.cc — BIT-EXACT cross-check: original msfa FmCore::render
 * vs FmCore_Render, over all 32 algorithms, gain-threshold edges, feedback
 * shifts (incl. >=16 pure fallback) and multi-block fb carry. Compares
 * outputs plus mutated params (gain_out/phase evolution). */
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "msfa_orig/fm_core.h"
#include "msfa_orig/exp2.h"
#include "msfa_orig/sin.h"
#include "fm_core.h"
#include "fm_exp2.h"
#include "fm_sin.h"

struct RefCore : public FmCore {
};

static uint32_t rng_state = 0x51ed2713u;
static uint32_t next_rand(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int32_t ref_out[64];
static int32_t port_out[64];

int main(void)
{
    RefCore rc;
    Sin::init();
    FmSin_Init();
    Exp2::init();
    FmExp2_Init();

    for (int alg = 0; alg < 32; alg++) {
        for (int trial = 0; trial < 12; trial++) {
            FmOpParams rp[6];
            FmOp portp[6];
            int32_t ref_fb[2] = {(int32_t)next_rand(), (int32_t)next_rand()};
            int32_t port_fb[2] = {ref_fb[0], ref_fb[1]};
            int32_t fb_gain = (int32_t)(next_rand() % 20);
            FmCoreState pcs;
            memset(&pcs, 0, sizeof(pcs));
            for (int op = 0; op < 6; op++) {
                /* levels cluster near the 1120 silence threshold edge. */
                int pick = (int)(next_rand() % 4);
                int32_t lvl = pick == 0 ? 1100 + (int32_t)(next_rand() % 40) :
                              (int32_t)(next_rand() & 0x1ffffffu);
                rp[op].level_in = lvl;
                portp[op].level_in = lvl;
                rp[op].gain_out = (int32_t)(next_rand() & 0x1ffffffu);
                portp[op].gain_out = rp[op].gain_out;
                rp[op].freq = (int32_t)(next_rand() & 0xffffffu);
                portp[op].freq = rp[op].freq;
                rp[op].phase = (int32_t)next_rand();
                portp[op].phase = rp[op].phase;
            }
            for (int blk = 0; blk < 3; blk++) {
                memset(ref_out, 0, sizeof(ref_out));
                memset(port_out, 0, sizeof(port_out));
                rc.render(ref_out, rp, alg, ref_fb, fb_gain);
                FmCore_Render(&pcs, port_out, portp, alg, port_fb,
                              fb_gain);
                for (int i = 0; i < 64; i++) {
                    if (ref_out[i] != port_out[i]) {
                        printf("DIFF alg=%d tr=%d blk=%d i=%d ref=%d port=%d\n",
                               alg, trial, blk, i, ref_out[i], port_out[i]);
                    }
                    assert(ref_out[i] == port_out[i]);
                }
                for (int op = 0; op < 6; op++) {
                    assert(rp[op].gain_out == portp[op].gain_out);
                    assert(rp[op].phase == portp[op].phase);
                }
                assert(ref_fb[0] == port_fb[0] && ref_fb[1] == port_fb[1]);
            }
        }
    }

    /* isCarrier / NumOutputs agree on all algorithms. */
    for (int alg = 0; alg < 32; alg++) {
        for (int op = 0; op < 6; op++) {
            assert((FmCore::isCarrier(alg, op) != 0) ==
                   (FmCore_IsCarrier(alg, op) != 0));
        }
    }

    printf("ALL FM CORE TESTS PASSED\n");
    return 0;
}
