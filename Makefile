CC_HOST  ?= gcc
CXX_HOST ?= c++
CC_PI    ?= pi32v2-gcc
# JieLi toolchain bin dir (ld/nm live next to cc). Override to match mount:
#   make fw CC_PI=/opt/jieli/pi32v2/bin/cc JIELI_BIN=/opt/jieli/pi32v2/bin
JIELI_BIN ?= /opt/jieli/pi32v2/bin
LD_PI     ?= $(JIELI_BIN)/ld
NM_PI     ?= $(JIELI_BIN)/nm
# Target libc/include tree (newlib math.h etc., same layout as the SDK).
PI_INC    := -I$(JIELI_BIN)/../include
CFLAGS   := -std=c99 -Wall -Wextra -Wsign-compare -Werror -Os -ffunction-sections -fdata-sections
SRC      := hal_shift_register.c sequencer.c audio_core.c bringup_probe.c debug_midi.c ota_guard.c
# FM engine port (Dexed/msfa -> C99). Not linked into firmware until the
# voice layer lands; target compiles below already gate it for pi32v2.
FM_SRC   := fm/fm_sin.c fm/fm_exp2.c fm/fm_freqlut.c

# Platform ABI: the ONLY undefined symbols our firmware may reference.
# Everything else (SDK boot, Timer/I2S registration, GPIOA MMIO) is either
# static-inline in bsp_config.h or provided by the JieLi link step.
# FM_*  = Dexed C++ engine port (extern "C" wrappers, linked later).
# Uart0_SendByte = probe-flash UART TX byte sink (probe image only).
# OTA_JumpToBootloader = board glue: quiesced-state jump into UBOOT OTA
#   (watchdog reset or retained-magic + system reset; must not return).
# memset/memcpy = JieLi libc (always present at the SDK link). Allowed
#   deliberately: clang lowers small constant-fill init loops to them
#   (e.g. 6-byte voice tables). They are bounded, heap-free, lock-free,
#   and ISR-safe. Anything ELSE undefined (float helpers, malloc, …) fails.
ABI_NORMAL := ^(FM_Init|FM_NoteOn|FM_NoteOff|FM_Render|OTA_JumpToBootloader|memset|memcpy)$$
ABI_PROBE  := ^(Uart0_SendByte|OTA_JumpToBootloader)$$

.PHONY: all host target fw image-dryrun check-no-malloc sweep-test sram clean

all: host

host: build/host_test build/edge_test build/probe_test build/ota_test build/ota_dispatch_test build/fm_tables_test build/fm_env_test build/fm_kernel_test build/fm_core_test build/fm_curve_test build/fm_note_test build/app_probe.o
	build/host_test
	build/edge_test
	build/probe_test
	build/ota_test
	build/ota_dispatch_test
	build/fm_tables_test
	build/fm_env_test
	build/fm_kernel_test
	build/fm_core_test
	build/fm_curve_test
	build/fm_note_test

build/host_test: $(SRC) tests/host_test.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/host_test.c $(SRC) -o $@

build/edge_test: $(SRC) tests/edge_test.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/edge_test.c $(SRC) -o $@

build/probe_test: tests/probe_test.c hal_shift_register.c bringup_probe.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/probe_test.c hal_shift_register.c bringup_probe.c -o $@

build/ota_test: tests/ota_test.c ota_guard.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/ota_test.c ota_guard.c -o $@

build/ota_dispatch_test: tests/ota_dispatch_test.c ota_dispatch.c ota_guard.c sequencer.c hal_shift_register.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/ota_dispatch_test.c ota_dispatch.c ota_guard.c sequencer.c hal_shift_register.c -o $@

build/fm_tables_test: tests/fm_tables_test.c $(FM_SRC) | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -I. tests/fm_tables_test.c $(FM_SRC) -o $@ -lm

# Bit-exact envelope cross-check: original msfa Env (C++) vs C99 port.
# Reference sources are vendored in tests/refcheck/ (verbatim upstream).
build/fm_env_test: tests/fm_env_test.cc fm/fm_env.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -c fm/fm_env.c -o build/fm_env_c.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST -Itests/refcheck -Ifm -I. -c tests/refcheck/msfa_orig/env.cc -o build/msfa_env_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST -Itests/refcheck -Ifm -I. tests/fm_env_test.cc build/fm_env_c.o build/msfa_env_ref.o -o $@

# Bit-exact operator-kernel cross-check (+ original sine table impl).
build/fm_kernel_test: tests/fm_kernel_test.cc fm/fm_sin.c fm/fm_op_kernel.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_sin.c -o build/fm_sin_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_op_kernel.c -o build/fm_kernel_c.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST -Itests/refcheck -c tests/refcheck/msfa_orig/sin.cc -o build/msfa_sin_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST -Itests/refcheck -c tests/refcheck/msfa_orig/fm_op_kernel.cc -o build/msfa_kernel_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST -Itests/refcheck -Ifm -I. tests/fm_kernel_test.cc build/fm_sin_c.o build/fm_kernel_c.o build/msfa_sin_ref.o build/msfa_kernel_ref.o -o $@ -lm

# Bit-exact core cross-check (threshold edges, fb carry, all algorithms).
build/fm_core_test: tests/fm_core_test.cc fm/fm_core.c fm/fm_exp2.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_core.c -o build/fm_ccore_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_exp2.c -o build/fm_cexp2_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_sin.c -o build/fm_csin_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_op_kernel.c -o build/fm_ckernel_c.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST $(REF_INC) -c tests/refcheck/msfa_orig/exp2.cc -o build/msfa_cexp2_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST $(REF_INC) -c tests/refcheck/msfa_orig/sin.cc -o build/msfa_csin_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST $(REF_INC) -c tests/refcheck/msfa_orig/fm_op_kernel.cc -o build/msfa_ckernel_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST $(REF_INC) -c tests/refcheck/msfa_orig/fm_core.cc -o build/msfa_ccore_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST $(REF_INC) -Wno-unused-private-field -Ifm -I. tests/fm_core_test.cc build/fm_ccore_c.o build/fm_cexp2_c.o build/fm_csin_c.o build/fm_ckernel_c.o build/msfa_cexp2_ref.o build/msfa_csin_ref.o build/msfa_ckernel_ref.o build/msfa_ccore_ref.o -o $@ -lm

# Tight unit test for the integer amp-mod-sens curve vs libm exp().
build/fm_curve_test: tests/fm_curve_test.c fm/fm_curve.c fm/fm_exp2.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_curve.c -o build/fm_curve_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_exp2.c -o build/fm_curvexp2_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -I. tests/fm_curve_test.c build/fm_curve_c.o build/fm_curvexp2_c.o -o $@ -lm

# Bit-exact voice cross-check: original Dx7Note vs C99 FmNote.
# Originals: dx7note/lfo/controllers/tuning-iface/porta + msfa DSP core,
# with JUCE-free TestTuning and null-safe MTS stubs (see test header).
# Tunings.h/TuningsImpl.h vendored from Surge tuning-library (MIT).
# Reference objects build WITHOUT -Werror (third-party code, newer-clang
# warnings); our files and the test driver keep it.
REF_INC := -Itests/refcheck
REF_CXX := $(CXX_HOST) -std=c++11 -Wall -Wextra -Os -DUNIT_TEST_HOST $(REF_INC)
build/fm_note_test: tests/fm_note_test.cc fm/fm_note.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_env.c -o build/fm_venv_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_lfo.c -o build/fm_vlfo_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_pitchenv.c -o build/fm_vpenv_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_porta.c -o build/fm_vporta_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_ctrl.c -o build/fm_vctrl_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_core.c -o build/fm_vcore_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_op_kernel.c -o build/fm_vkernel_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_sin.c -o build/fm_vsin_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_exp2.c -o build/fm_vexp2_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_freqlut.c -o build/fm_vfreq_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_note.c -o build/fm_vnote_c.o
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -Ifm -c fm/fm_curve.c -o build/fm_vcurve_c.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/env.cc -o build/msfa_venv_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/sin.cc -o build/msfa_vsin_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/exp2.cc -o build/msfa_vexp2_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/freqlut.cc -o build/msfa_vfreq_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/fm_op_kernel.cc -o build/msfa_vkernel_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/fm_core.cc -o build/msfa_vcore_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/lfo.cc -o build/msfa_vlfo_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/pitchenv.cc -o build/msfa_vpenv_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/porta.cpp -o build/msfa_vporta_ref.o
	$(REF_CXX) -c tests/refcheck/msfa_orig/dx7note.cc -o build/msfa_vnote_ref.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Os -DUNIT_TEST_HOST $(REF_INC) -Wno-unused-private-field -c tests/refcheck/ref_fb_zero.cc -o build/ref_fb_zero.o
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -Wno-unused-private-field -Os -DUNIT_TEST_HOST $(REF_INC) -Ifm -I. tests/fm_note_test.cc build/fm_v*.o build/msfa_v*.o build/ref_fb_zero.o -o $@ -lm

# Probe-flash app variant (bring-up only): compile-checked, never run on host.
build/app_probe.o: app_main.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -DBRINGUP_PROBE -I. -c app_main.c -o $@

build:
	mkdir -p build

# Independent per-module cross-compiles: catches pi32v2 syntax errors early.
# NOTE: link happens against the Dexed C++ engine port (extern "C" wrappers
# matching fm_stub.h) — these -c compiles intentionally stop before link.
target: | build
	$(CC_PI) $(CFLAGS) $(PI_INC) -c hal_shift_register.c -o build/hal.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c sequencer.c -o build/seq.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c audio_core.c -o build/audio.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c app_main.c -o build/app.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -DBRINGUP_PROBE -c app_main.c -o build/app_probe.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c debug_midi.c -o build/debug.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c bringup_probe.c -o build/probe.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c ota_guard.c -o build/ota.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c ota_dispatch.c -o build/ota_dispatch.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_sin.c -o build/fm_sin.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_exp2.c -o build/fm_exp2.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_freqlut.c -o build/fm_freqlut.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_env.c -o build/fm_env.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_lfo.c -o build/fm_lfo.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_pitchenv.c -o build/fm_pitchenv.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_porta.c -o build/fm_porta.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_ctrl.c -o build/fm_ctrl.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_core.c -o build/fm_core.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_op_kernel.c -o build/fm_op_kernel.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_curve.c -o build/fm_curve.o
	$(CC_PI) $(CFLAGS) $(PI_INC) -c fm/fm_note.c -o build/fm_note.o

check-no-malloc:
	! grep -rnE '\b(malloc|calloc|realloc|free)\s*\(' --include='*.c' --include='*.h' --exclude-dir=build .

sweep-test:
	python3 tools/decode_sweep.py --selftest

# Firmware partial-link: merges our objects into relocatable firmware images
# and gates on the ABI allowlist — any unexpected undefined symbol (missing
# helper, accidental libc call like memcpy, soft-float libcall) fails here,
# long before the JieLi SDK link. Requires the toolchain (runs in Docker).
FW_NORMAL_OBJS := build/hal.o build/seq.o build/audio.o build/app.o build/debug.o build/ota.o build/ota_dispatch.o
FW_PROBE_OBJS  := build/hal.o build/probe.o build/app_probe.o build/ota.o build/ota_dispatch.o

fw: target
	$(LD_PI) -r $(FW_NORMAL_OBJS) -o build/fm1-polyseq.o
	$(LD_PI) -r $(FW_PROBE_OBJS) -o build/fm1-probe.o
	$(NM_PI) -u build/fm1-polyseq.o | awk '{print $$2}' | sort -u > build/undef-normal.txt
	$(NM_PI) -u build/fm1-probe.o | awk '{print $$2}' | sort -u > build/undef-probe.txt
	@echo "== undefined in fm1-polyseq.o =="; cat build/undef-normal.txt
	@echo "== undefined in fm1-probe.o =="; cat build/undef-probe.txt
	! grep -vE '$(ABI_NORMAL)' build/undef-normal.txt | grep -q .
	! grep -vE '$(ABI_PROBE)' build/undef-probe.txt | grep -q .
	@echo "ABI GATE OK"
	size build/fm1-polyseq.o build/fm1-probe.o

# Image-layout dry run: link the normal firmware against DRY-RUN ONLY
# stubs (target/stubs.c) with the draft script (target/fm1.ld) and prove
# placement — code in XIP rom (0x2000...), data/bss in ram (0x1c0...).
# Requires the toolchain (runs in Docker/CI); not a bootable image.
image-dryrun: fw
	$(CC_PI) $(CFLAGS) -fno-builtin-memset -fno-builtin-memcpy -c target/stubs.c -o build/stubs.o
	$(LD_PI) -T target/fm1.ld build/fm1-polyseq.o build/stubs.o -o build/fm1-dryrun.elf -Map build/fm1-dryrun.map
	$(NM_PI) build/fm1-dryrun.elf | grep -q '^0*2000[0-9a-f]* T main' || (echo "main not in rom"; exit 1)
	! $(NM_PI) build/fm1-dryrun.elf | grep -E '^[0-9a-f]+ [TtRr] ' | grep -vq '^0*2000' || (echo "code/rodata outside rom"; exit 1)
	! $(NM_PI) build/fm1-dryrun.elf | grep -E '^[0-9a-f]+ [BbDd] ' | grep -vq '^0*1c0' || (echo "data/bss outside ram"; exit 1)
	@echo "IMAGE LAYOUT OK"
	size build/fm1-dryrun.elf

sram:
	nm --print-size --size-sort build/*.o 2>/dev/null | tail -20 || true

# NOTE: build/ also hosts gitignored reference trees (toolchain/, sdk/)
# that are expensive to re-fetch. clean removes build products only.
clean:
	rm -rf build/*.o build/*_test build/host_test build/*.elf build/*.map build/undef-*.txt build/fm1-*.o *.o *.gch
