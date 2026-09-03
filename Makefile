CC_HOST  ?= gcc
CC_PI    ?= pi32v2-gcc
# JieLi toolchain bin dir (ld/nm live next to cc). Override to match mount:
#   make fw CC_PI=/opt/jieli/pi32v2/bin/cc JIELI_BIN=/opt/jieli/pi32v2/bin
JIELI_BIN ?= /opt/jieli/pi32v2/bin
LD_PI     ?= $(JIELI_BIN)/ld
NM_PI     ?= $(JIELI_BIN)/nm
CFLAGS   := -std=c99 -Wall -Wextra -Wsign-compare -Werror -Os -ffunction-sections -fdata-sections
SRC      := hal_shift_register.c sequencer.c audio_core.c bringup_probe.c debug_midi.c

# Platform ABI: the ONLY undefined symbols our firmware may reference.
# Everything else (SDK boot, Timer/I2S registration, GPIOA MMIO) is either
# static-inline in bsp_config.h or provided by the JieLi link step.
# FM_*  = Dexed C++ engine port (extern "C" wrappers, linked later).
# Uart0_SendByte = probe-flash UART TX byte sink (probe image only).
# memset/memcpy = JieLi libc (always present at the SDK link). Allowed
#   deliberately: clang lowers small constant-fill init loops to them
#   (e.g. 6-byte voice tables). They are bounded, heap-free, lock-free,
#   and ISR-safe. Anything ELSE undefined (float helpers, malloc, …) fails.
ABI_NORMAL := ^(FM_Init|FM_NoteOn|FM_NoteOff|FM_Render|memset|memcpy)$$
ABI_PROBE  := ^(Uart0_SendByte)$$

.PHONY: all host target fw image-dryrun check-no-malloc sweep-test sram clean

all: host

host: build/host_test build/edge_test build/probe_test build/app_probe.o
	build/host_test
	build/edge_test
	build/probe_test

build/host_test: $(SRC) tests/host_test.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/host_test.c $(SRC) -o $@

build/edge_test: $(SRC) tests/edge_test.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/edge_test.c $(SRC) -o $@

build/probe_test: tests/probe_test.c hal_shift_register.c bringup_probe.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/probe_test.c hal_shift_register.c bringup_probe.c -o $@

# Probe-flash app variant (bring-up only): compile-checked, never run on host.
build/app_probe.o: app_main.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -DBRINGUP_PROBE -I. -c app_main.c -o $@

build:
	mkdir -p build

# Independent per-module cross-compiles: catches pi32v2 syntax errors early.
# NOTE: link happens against the Dexed C++ engine port (extern "C" wrappers
# matching fm_stub.h) — these -c compiles intentionally stop before link.
target: | build
	$(CC_PI) $(CFLAGS) -c hal_shift_register.c -o build/hal.o
	$(CC_PI) $(CFLAGS) -c sequencer.c -o build/seq.o
	$(CC_PI) $(CFLAGS) -c audio_core.c -o build/audio.o
	$(CC_PI) $(CFLAGS) -c app_main.c -o build/app.o
	$(CC_PI) $(CFLAGS) -DBRINGUP_PROBE -c app_main.c -o build/app_probe.o
	$(CC_PI) $(CFLAGS) -c debug_midi.c -o build/debug.o
	$(CC_PI) $(CFLAGS) -c bringup_probe.c -o build/probe.o

check-no-malloc:
	! grep -rnE '\b(malloc|calloc|realloc|free)\s*\(' --include='*.c' --include='*.h' --exclude-dir=build .

sweep-test:
	python3 tools/decode_sweep.py --selftest

# Firmware partial-link: merges our objects into relocatable firmware images
# and gates on the ABI allowlist — any unexpected undefined symbol (missing
# helper, accidental libc call like memcpy, soft-float libcall) fails here,
# long before the JieLi SDK link. Requires the toolchain (runs in Docker).
FW_NORMAL_OBJS := build/hal.o build/seq.o build/audio.o build/app.o build/debug.o
FW_PROBE_OBJS  := build/hal.o build/probe.o build/app_probe.o

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

clean:
	rm -rf build *.o *.gch
