#!/usr/bin/env python3
"""Decode FM-1 front-panel probe sweep logs into shift-register bit roles.

Capture procedure (probe firmware, -DBRINGUP_PROBE, ~100 ms/step over UART):
  1. No keys pressed  -> baseline.log   (one full 32-line sweep)
  2. Hold key K       -> pressed_K.log  (one full sweep per key probed)

Each sweep line is either  "P<bb> IN=<hh>"  (shift bit bb, column field hh)
or "LED<bb>" (LED walk echo, ignored for mapping).

Row-drive discovery: a shift bit that shows a DIFFERENT column field when a
key is held (vs baseline) drives the matrix row containing that key. Bits
that never respond are LED/spare candidates (correlate visually with the
LED walk, then confirm by lighting them one at a time).

Usage:
    python3 tools/decode_sweep.py baseline.log pressed_*.log
    python3 tools/decode_sweep.py --selftest   # embedded synthetic checks

Exit status: 0 on clean decode, 1 on parse errors or inconsistent repeats.
Prints suggested bsp_config.h masks on stdout (human-readable report).
"""

import re
import sys

SHIFT_RE = re.compile(r"^P(\d{2}) IN=([0-9A-Fa-f]{2})$")
LED_RE = re.compile(r"^LED(\d{2})$")
N_SHIFT_BITS = 16


def parse_log(path):
    """Return {bit: cols} for shift lines. Repeated bits must agree."""
    found = {}
    leds = 0
    with open(path) as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line:
                continue
            m = SHIFT_RE.match(line)
            if m:
                bit, cols = int(m.group(1)), int(m.group(2), 16)
                if bit >= N_SHIFT_BITS:
                    raise ValueError("%s:%d: bit %d out of range" %
                                     (path, lineno, bit))
                if bit in found and found[bit] != cols:
                    raise ValueError(
                        "%s:%d: bit %02d inconsistent (%02X vs %02X)" %
                        (path, lineno, bit, found[bit], cols))
                found[bit] = cols
                continue
            if LED_RE.match(line):
                leds += 1
                continue
            raise ValueError("%s:%d: unparsable line %r" % (path, lineno, line))
    return found, leds


def decode(baseline, pressed_list):
    """pressed_list: [(label, {bit: cols})]. Returns report dict."""
    row_bits = set()
    per_key = {}
    for label, sweep in pressed_list:
        hits = {}
        for bit in sorted(set(baseline) | set(sweep)):
            base = baseline.get(bit)
            cur = sweep.get(bit)
            if base is None or cur is None:
                continue  # bit missing from one capture; ignore
            if cur != base:
                hits[bit] = (base, cur)
                row_bits.add(bit)
        per_key[label] = hits
    quiet = sorted(b for b in baseline if b not in row_bits)
    return {"row_bits": sorted(row_bits), "quiet_bits": quiet,
            "per_key": per_key}


def report_text(rep):
    out = []
    out.append("row-drive shift bits : %s" %
               (" ".join("%02d" % b for b in rep["row_bits"]) or "(none)"))
    out.append("quiet bits (LED/spare candidates): %s" %
               (" ".join("%02d" % b for b in rep["quiet_bits"]) or "(none)"))
    mask = 0
    for b in rep["row_bits"]:
        mask |= (1 << b)
    out.append("suggested SR_ROW_SHIFT_MASK: 0x%04X" % mask)
    for label, hits in rep["per_key"].items():
        if hits:
            detail = ", ".join(
                "bit%02d: %02X->%02X" % (b, a, c)
                for b, (a, c) in sorted(hits.items()))
        else:
            detail = "no change vs baseline (key not in matrix or bad capture)"
        out.append("key %-12s : %s" % (label, detail))
    if len(rep["row_bits"]) > 6:
        out.append("WARNING: >6 responding bits; check for floating columns "
                   "or multiple keys held.")
    return "\n".join(out) + "\n"


def selftest():
    import io
    import os
    import tempfile

    def write_tmp(lines):
        fd, path = tempfile.mkstemp(suffix=".log")
        with os.fdopen(fd, "w") as fh:
            fh.write("".join(l + "\n" for l in lines))
        return path

    def sweep(cols_by_bit):
        return ["P%02d IN=%02X" % (b, cols_by_bit.get(b, 0)) for b in
                range(N_SHIFT_BITS)] + ["LED%02d" % b for b in
                                        range(N_SHIFT_BITS)]

    base = {b: 0x00 for b in range(N_SHIFT_BITS)}
    # Row bit 2 carries key A on column 3; row bit 5 carries key B on col 0.
    pa = dict(base)
    pa[2] = 0x08
    pb = dict(base)
    pb[5] = 0x01
    p_base, p_a, p_b = (write_tmp(sweep(m)) for m in (base, pa, pb))
    try:
        b, _ = parse_log(p_base)
        rep = decode(b, [("keyA", parse_log(p_a)[0]),
                         ("keyB", parse_log(p_b)[0])])
        assert rep["row_bits"] == [2, 5], rep
        assert rep["per_key"]["keyA"] == {2: (0x00, 0x08)}, rep
        assert rep["per_key"]["keyB"] == {5: (0x00, 0x01)}, rep
        assert rep["quiet_bits"] == [b for b in range(16) if b not in (2, 5)]
        text = report_text(rep)
        assert "SR_ROW_SHIFT_MASK: 0x0024" in text, text
        # Inconsistent repeat must raise.
        p_bad = write_tmp(["P02 IN=00", "P02 IN=01"])
        try:
            parse_log(p_bad)
        except ValueError:
            pass
        else:
            raise AssertionError("inconsistent repeat not detected")
        # Garbage line must raise.
        p_garbage = write_tmp(["P02 IN=00", "hello"])
        try:
            parse_log(p_garbage)
        except ValueError:
            pass
        else:
            raise AssertionError("garbage line not detected")
    finally:
        for p in (p_base, p_a, p_b, p_bad, p_garbage):
            os.unlink(p)
    sys.stdout.write("SELFTEST OK\n")


def main(argv):
    if argv == ["--selftest"]:
        selftest()
        return 0
    if len(argv) < 2:
        sys.stderr.write("usage: decode_sweep.py baseline.log "
                         "pressed_<key>.log [pressed_<key>.log ...]\n")
        return 1
    try:
        baseline, _ = parse_log(argv[0])
        pressed = []
        for path in argv[1:]:
            label = path.rsplit("/", 1)[-1]
            if label.startswith("pressed_"):
                label = label[len("pressed_"):]
            if label.endswith(".log"):
                label = label[:-len(".log")]
            pressed.append((label, parse_log(path)[0]))
    except (ValueError, OSError) as exc:
        sys.stderr.write("error: %s\n" % exc)
        return 1
    if len(baseline) < N_SHIFT_BITS:
        sys.stderr.write("warning: baseline covers %d/16 shift bits "
                         "(partial sweep?)\n" % len(baseline))
    sys.stdout.write(report_text(decode(baseline, pressed)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
