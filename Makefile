CC_HOST  ?= gcc
CC_PI    ?= pi32v2-gcc
CFLAGS   := -std=c99 -Wall -Wextra -Werror -Os -ffunction-sections -fdata-sections
SRC      := hal_shift_register.c sequencer.c audio_core.c bringup_probe.c debug_midi.c

.PHONY: all host target check-no-malloc sram clean

all: host

host: build/host_test build/edge_test build/probe_test
	build/host_test
	build/edge_test
	build/probe_test

build/host_test: $(SRC) tests/host_test.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/host_test.c $(SRC) -o $@

build/edge_test: $(SRC) tests/edge_test.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/edge_test.c $(SRC) -o $@

build/probe_test: tests/probe_test.c hal_shift_register.c bringup_probe.c | build
	$(CC_HOST) $(CFLAGS) -DUNIT_TEST_HOST -I. tests/probe_test.c hal_shift_register.c bringup_probe.c -o $@

build:
	mkdir -p build

# Independent per-module cross-compiles: catches pi32v2 syntax errors early.
# NOTE: link happens against the Dexed C++ engine port (extern "C" wrappers
# matching fm_stub.h) — these -c compiles intentionally stop before link.
target:
	$(CC_PI) $(CFLAGS) -c hal_shift_register.c -o build/hal.o
	$(CC_PI) $(CFLAGS) -c sequencer.c -o build/seq.o
	$(CC_PI) $(CFLAGS) -c audio_core.c -o build/audio.o
	$(CC_PI) $(CFLAGS) -c app_main.c -o build/app.o
	$(CC_PI) $(CFLAGS) -c debug_midi.c -o build/debug.o
	$(CC_PI) $(CFLAGS) -c bringup_probe.c -o build/probe.o

check-no-malloc:
	! grep -rnE '\b(malloc|calloc|realloc|free)\s*\(' --include='*.c' --include='*.h' --exclude-dir=build .

sram:
	nm --print-size --size-sort build/*.o 2>/dev/null | tail -20 || true

clean:
	rm -rf build *.o *.gch
