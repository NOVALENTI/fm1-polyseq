/* TEMP-DEBUG driver (not committed): first-case reproduction. */
#include <stdio.h>
#include <string.h>
#include <memory>
#include "msfa_orig/dx7note.h"
#include "msfa_orig/lfo.h"
#include "msfa_orig/controllers.h"
#include "msfa_orig/tuning.h"
#include "msfa_orig/fm_core.h"
#include "msfa_orig/sin.h"
#include "msfa_orig/exp2.h"
#include "msfa_orig/freqlut.h"
#include "msfa_orig/env.h"
#include "msfa_orig/pitchenv.h"
#include "fm_note.h"
#include "fm_sin.h"
#include "fm_exp2.h"
#include "fm_freqlut.h"
#include "fm_env.h"
#include "fm_pitchenv.h"
#include "fm_lfo.h"
#include "fm_ctrl.h"
#include "fm_core.h"
#include "fm_porta.h"

bool MTS_HasMaster(MTSClient *c)
{
    (void)c;
    return false;
}

double MTS_NoteToFrequency(MTSClient *c, char midinote, char midichannel)
{
    (void)c;
    (void)midinote;
    (void)midichannel;
    return 0.0;
}

struct TestTuning : public TuningState {
    virtual bool is_standard_tuning() override
    {
        return true;
    }
    virtual int32_t midinote_to_logfreq(int midinote) override
    {
        return 50857777 + ((1 << 24) / 12) * midinote;
    }
    virtual int scale_length() override
    {
        return 12;
    }
    virtual std::string display_tuning_str() override
    {
        return std::string("test");
    }
    virtual Tunings::Tuning &getTuning() override
    {
        return tuning_;
    }
    Tunings::Tuning tuning_;
};

struct RefCore : public FmCore {
};

/* Real Layer-A first-case patch (post-override alg=0, fb=0). */
static const uint8_t kPatch[156] = {
    25, 62, 58, 81, 31, 55, 8, 91, 60, 184, 238, 180, 211, 4, 184, 95,
    38, 0, 29, 63, 11, 22, 16, 62, 28, 73, 34, 42, 25, 60, 166, 154,
    107, 65, 7, 120, 126, 26, 0, 31, 57, 10, 41, 80, 18, 1, 61, 66,
    19, 16, 60, 253, 125, 68, 15, 0, 116, 229, 41, 0, 20, 52, 1, 92,
    83, 7, 26, 36, 76, 71, 44, 60, 203, 183, 199, 192, 7, 40, 118, 1,
    1, 31, 51, 14, 32, 64, 34, 16, 27, 8, 41, 5, 60, 9, 15, 248, 27,
    4, 236, 221, 2, 1, 29, 66, 7, 12, 22, 91, 69, 53, 24, 85, 79, 60,
    215, 120, 131, 101, 3, 140, 22, 87, 0, 27, 77, 0, 29, 93, 29, 20,
    53, 16, 14, 72, 174, 5, 198, 141, 47, 145, 144, 120, 93, 116, 116,
    25, 194, 244, 119, 46, 49, 236, 43, 0, 22, 6
};

static int32_t ref_buf[64];
static int32_t port_buf[64];

int main(void)
{
    std::shared_ptr<TuningState> ts(new TestTuning());
    uint8_t patch[156];
    uint8_t lfo_params[6] = {50, 0, 0, 0, 1, 0};
    Dx7Note ref(ts, nullptr);
    FmNote port;
    RefCore rc;
    FmCoreState pcs;
    Controllers rctrls;
    FmCtrl pctrls;
    Lfo rlfo;
    FmLfo plfo;
    int nd = 0;

    memcpy(patch, kPatch, sizeof(patch));
    patch[134] = 0;
    patch[135] = 0;
    Sin::init();
    FmSin_Init();
    Exp2::init();
    FmExp2_Init();
    Tanh::init();
    FmTanh_Init();
    Freqlut::init(48000.0);
    FmFreqlut_Init(48000.0);
    Env::init_sr(48000.0);
    FmEnv_InitSr(48000.0);
    PitchEnv::init(48000.0);
    FmPitchEnv_InitSr(48000.0);
    Lfo::init(48000.0);
    FmLfo_Init(48000.0);
    Porta::init_sr(48000.0);
    FmPorta_InitSr(48000.0);
    memset(&pcs, 0, sizeof(pcs));
    FmCtrl_Init(&pctrls);
    for (int i = 0; i < 132; i++) {
        rctrls.values_[i] = 0;
    }
    rctrls.values_[128] = 0x2000;
    rctrls.portamento_enable_cc = 0;
    rctrls.portamento_gliss_cc = 0;
    rctrls.portamento_cc = 0;
    rctrls.masterTune = 0;
    rctrls.refresh();
    FmCtrl_Refresh(&pctrls);
    rctrls.core = &rc;
    ref.init(patch, 60, 100, 1, &rctrls);
    FmNote_Init(&port, patch, 60, 100, 1, &pctrls);
    rlfo.reset(lfo_params);
    FmLfo_Reset(&plfo, lfo_params);
    rlfo.keydown();
    FmLfo_KeyDown(&plfo);
    for (int blk = 0; blk < 3; blk++) {
        int32_t rlfo_val = rlfo.getsample();
        int32_t rlfo_dly = rlfo.getdelay();
        int32_t plfo_val = FmLfo_GetSample(&plfo);
        int32_t plfo_dly = FmLfo_GetDelay(&plfo);
        if (rlfo_val != plfo_val || rlfo_dly != plfo_dly) {
            printf("LFO DIFF\n");
            return 1;
        }
        memset(ref_buf, 0, sizeof(ref_buf));
        memset(port_buf, 0, sizeof(port_buf));
        ref.compute(ref_buf, rlfo_val, rlfo_dly, &rctrls);
        FmNote_Compute(&port, &pcs, port_buf, plfo_val, plfo_dly, &pctrls);
        for (int i = 0; i < 64; i++) {
            if (ref_buf[i] != port_buf[i]) {
                if (nd < 6) {
                    printf("blk%d i=%d ref=%d port=%d\n", blk, i, ref_buf[i],
                           port_buf[i]);
                }
                nd++;
            }
        }
    }
    printf("ndiff=%d\n", nd);
    return 0;
}
