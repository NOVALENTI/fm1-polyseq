/* FM leaf-table tests: exact anchors + tolerance + determinism.
 * These pin the C99 port to upstream msfa behavior. */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "fm_sin.h"
#include "fm_exp2.h"
#include "fm_freqlut.h"

static int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

int main(void)
{
    int32_t a, b;

    FmSin_Init();
    FmExp2_Init();
    FmTanh_Init();
    FmFreqlut_Init(48000.0);

    /* Sin: exact zero at phase 0 (table seed is exact). */
    assert(FmSin_Lookup(0) == 0);
    /* Quarter circle (phase 2^22 of 2^24) ~= Q24 1.0; tolerance covers
     * rotation-accumulation error in the boot table (exact value is
     * table-defined, behavior here is range + symmetry). */
    a = FmSin_Lookup(1 << 22);
    assert(iabs32(a - (1 << 24)) < 64);
    /* Half circle is exactly 0 (seed row); three-quarter ~= -Q24. */
    assert(FmSin_Lookup(1 << 23) == 0);
    assert(iabs32(FmSin_Lookup(3 << 22) + (1 << 24)) < 64);
    /* Antisymmetry sin(p) == -sin(p + half), up to 1 LSB: the
     * interpolation uses arithmetic right shift (floor), so negated
     * products can differ by one unit in the last place. Same upstream. */
    a = FmSin_Lookup(1234567);
    b = FmSin_Lookup(1234567 + (1 << 23));
    assert(iabs32(a + b) <= 1);

    /* Exp2: x=0 -> Q24 1.0 exactly (table seed is exact). */
    assert(FmExp2_Lookup(0) == (1 << 24));
    /* Doubling per octave: exp2(1<<24) == 2 * exp2(0). */
    assert(FmExp2_Lookup(1 << 24) == 2 * FmExp2_Lookup(0));
    /* Monotonic over [0, 2 octaves]. */
    a = FmExp2_Lookup(0);
    for (int32_t x = 100000; x < (2 << 24); x += 100000) {
        b = FmExp2_Lookup(x);
        assert(b >= a);
        a = b;
    }

    /* Tanh: exact zero at 0. At exactly 4<<24 upstream takes the exp2
     * identity path (exact saturation only above 8.5<<24), so range-check:
     * tanh(4) ~= 0.9993, i.e. within 32768 of Q24 1.0. Negative side lands
     * one LSB below the boundary and interpolates (upstream quirk), so
     * range-check it too. Odd symmetry holds within 2 LSB (sign-flip +
     * lowbit interpolation rounding; measured 0 here, tolerance guards
     * against sign/path regressions, not LSB identity). */
    assert(FmTanh_Lookup(0) == 0);
    a = FmTanh_Lookup(4 << 24);
    assert(a > (1 << 24) - 32768 && a <= (1 << 24));
    b = FmTanh_Lookup(-(4 << 24));
    assert(b < 0 && b > -(1 << 24));
    a = FmTanh_Lookup(1 << 24);
    assert(a > 0 && a < (1 << 24));
    assert(iabs32(a + FmTanh_Lookup(-(1 << 24))) <= 2);

    /* Freqlut: positive deltas, octave doubling up to the truncated bit
     * (y>>19 vs 2*(y>>20) differ by bit 19 of y — same upstream), 1.5
     * octaves lands between, deterministic. */
    a = FmFreqlut_Lookup(0);
    assert(a > 0);
    assert(FmFreqlut_Lookup(0) == a);
    b = FmFreqlut_Lookup(1 << 24); /* +1 octave */
    assert(b == 2 * a || b == 2 * a + 1);
    b = FmFreqlut_Lookup((1 << 24) + (1 << 23)); /* +1.5 octaves */
    assert(b > 2 * a && b < 3 * a);

    printf("ALL FM TABLE TESTS PASSED\n");
    return 0;
}
