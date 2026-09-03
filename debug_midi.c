/* debug_midi.c — drain due FIFO events and print one line each over UART.
 * Format per event: "T<due_us> <ON|OFF> V<voice> N<note> V<vel>\n" (decimal).
 * Runs in main-loop context only (uses the audio-side consumer endpoint with
 * the sequencer clock as horizon). Static buffer, no heap, no stdio. */
#include "debug_midi.h"
#include "sequencer.h"

static void put_str(Debug_PutcFn f, const char *s)
{
    while (*s) {
        f(*s++);
    }
}

static void put_u32(Debug_PutcFn f, uint32_t v)
{
    char tmp[10];
    int n = 0;
    if (v == 0u) {
        f('0');
        return;
    }
    while (v > 0u && n < 10) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0) {
        f(tmp[--n]);
    }
}

void Debug_DumpPendingEvents(Debug_PutcFn putc_fn)
{
    MidiEvent_t ev;
    if (putc_fn == 0) {
        return;
    }
    while (SEQ_FIFO_PopDue(Sequencer_NowUs(), &ev)) {
        put_str(putc_fn, "T");
        put_u32(putc_fn, ev.due_us);
        put_str(putc_fn, (ev.type == MIDI_NOTE_ON) ? " ON V" : " OFF V");
        put_u32(putc_fn, ev.voice_id);
        put_str(putc_fn, " N");
        put_u32(putc_fn, ev.note);
        put_str(putc_fn, " V");
        put_u32(putc_fn, ev.vel);
        put_str(putc_fn, "\n");
    }
}
