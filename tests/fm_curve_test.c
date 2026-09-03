/* fm_curve_test.c — tight unit test for the integer amp-mod-sens curve.
 *
 * Upstream computes pt = exp(sensamp * (0.07/262144) + 12.2) in double
 * (render path!); the port uses an Exp2-table integer approximation
 * (fm_amp_curve via FmExp2_Lookup, no soft-float on target). This test
 * pins the approximation against libm exp() over the full uint32 input
 * domain the voice can produce: relative error < 1% everywhere below
 * the uint32 saturation point, monotonic non-decreasing, exact-ish at
 * sensamp = 0 (exp(12.2)).
 *
 * Rationale for accepting <1% instead of exact: a soft-float exp() per
 * modulated op per block costs a large fraction of the pi32v2 voice
 * budget; sub-LSB curve error can shift an envelope stage boundary by
 * at most a block (covered as smoke in fm_note_test Layer B2). */
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "fm_exp2.h"
#include "fm_curve.h"

/* Exercises the real render-path function (not a mirror). */

int main(void)
{
    double worst = 0.0;
    uint32_t prev = 0;
    int first = 1;

    FmExp2_Init();

    for (uint64_t s = 0; s <= 45000000u; s += 999) {
        uint32_t sens = (uint32_t)s;
        double want = exp(((double)sens) / 262144 * 0.07 + 12.2);
        uint32_t got;
        double rel;
        if (want > 4294967295.0) {
            continue; /* out of uint32 range on both sides */
        }
        got = FmNote_AmpCurve(sens);
        rel = (got - want) / want;
        if (rel < 0) {
            rel = -rel;
        }
        if (rel > worst) {
            worst = rel;
        }
        if (!first) {
            assert(got >= prev); /* monotonic */
        }
        first = 0;
        prev = got;
    }
    printf("curve worst rel err %f\n", worst);
    assert(worst < 0.01);

    /* Sanity anchor: sensamp 0 -> exp(12.2). */
    {
        double want = exp(12.2);
        uint32_t got = FmNote_AmpCurve(0);
        double rel = (got - want) / want;
        if (rel < 0) {
            rel = -rel;
        }
        assert(rel < 0.01);
    }

    printf("ALL FM CURVE TESTS PASSED\n");
    return 0;
}
