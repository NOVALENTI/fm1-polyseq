#ifndef REF_FB_ZERO_H
#define REF_FB_ZERO_H

/* Test-only accessor: zeroes the reference Dx7Note feedback delay line.
 * Upstream leaves fb_buf_ uninitialized (implicit ctor); the feedback
 * operator reads it on the first block, making stock first-block output
 * with feedback active nondeterministic (stack garbage, converges in a
 * few samples). Our firmware always starts from zeroed state, so the
 * cross-check zeroes the reference side too — comparing defined behavior
 * against defined behavior. See fm_note_test run_note. */

class Dx7Note;

void RefZeroFb(Dx7Note *note);

#endif
