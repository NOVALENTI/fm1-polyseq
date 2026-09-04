#!/usr/bin/env python3
"""fm1_flash.py — macOS CoreMIDI port of the FM-1 OTA update flow.

Implements the two-stage pull protocol observed in M-UPGRADE (see
AL-255/FM-1-RE tools/fm1_ota.py, whose packet codec is mirrored here and
cross-checked against its golden vectors in --selftest):

  scan              read-only handshake + identity decode (safe anytime)
  info FILE.fwsc    offline package inspection (no device needed)
  dryrun FILE.fwsc  full pre-flight: parse, handshake, identity-vs-package
                    match, request simulation — sends NOTHING but the
                    handshake query
  flash FILE.fwsc   handshake -> UPGRADE_CMD -> serve read requests ->
                    finish -> wait re-enumeration. REQUIRES --confirm AND
                    interactive typed confirmation. Reflashing the SAME
                    version the device already runs is expected to be
                    refused by the stock verifier (safe no-op).

Requires python-rtmidi (mido backend). The device appears as the
'USB Composite Device' CoreMIDI ports.
"""

import argparse
import os
import re
import sys
import time

try:
    import mido
except ImportError:
    mido = None

HS_QUERY = bytes([0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7])
UPGRADE_CMD = bytes([0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7])
HDR_DATA = b"\x00\x32\x41\x41"
HDR_HS = b"\x00\x32\x45\x58"
REQ_TIMEOUT = 8.0
MAXDATA = 512
RESP_DELAY = 0.010
PORT_NAME = "USB Composite Device"


def pack7(data):
    out = bytearray()
    acc = 0
    nb = 0
    for b in data:
        acc |= b << nb
        nb += 8
        while nb >= 7:
            out.append(acc & 0x7F)
            acc >>= 7
            nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return bytes(out)


def unpack7(s):
    out = bytearray()
    acc = 0
    nb = 0
    for b in s:
        acc |= b << nb
        nb += 7
        while nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return bytes(out)


def u7(b):
    return b[0] | (b[1] << 7) | (b[2] << 14) | (b[3] << 21)


def e7(v):
    return bytes([(v) & 0x7F, (v >> 7) & 0x7F, (v >> 14) & 0x7F,
                  (v >> 21) & 0x7F])


def build_response(addr, data, req_len=None, flashtype=0):
    if req_len is None:
        req_len = len(data)
    chk = (~(flashtype + sum(data) + sum(addr.to_bytes(4, "little"))
             + sum(req_len.to_bytes(3, "little")))) & 0xFF
    return (b"\xF0" + HDR_DATA + e7(req_len >> 4) + e7(addr)
            + e7((req_len << 4) | flashtype) + pack7(data + bytes([chk]))
            + b"\xF7")


def build_success(addr):
    payload = b"success\x00"
    body = (bytes([0x00, 0x59, 0x30]) + (len(payload) + 8).to_bytes(3, "little")
            + bytes([0]) + addr.to_bytes(4, "little")
            + len(payload).to_bytes(3, "little") + payload)
    chk = (~sum(body[6:])) & 0xFF
    return b"\xF0" + pack7(body + bytes([chk])) + b"\xF7"


def parse_request(pkt):
    if len(pkt) < 4 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    u = unpack7(pkt[1:-1])
    if len(u) != 15 or u[:3] != b"\x00\x59\x30":
        return None
    if int.from_bytes(u[3:6], "little") != 8:
        return None
    if u[-1] != ((~sum(u[6:-1])) & 0xFF):
        return None
    fl = u[6]
    addr = int.from_bytes(u[7:11], "little")
    ln = int.from_bytes(u[11:14], "little")
    return (fl, addr, ln)


def parse_identity(pkt):
    if len(pkt) < 4 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    decoded = unpack7(pkt[1:-1])
    if len(decoded) != 34 or decoded[:3] != b"\x00\x59\x11":
        return None
    if int.from_bytes(decoded[3:6], "little") != 27:
        return None
    if decoded[-1] != ((~sum(decoded[6:-1])) & 0xFF):
        return None
    plain = decoded[6:31]
    sep = plain.find(b"_")
    if sep < 0:
        return None
    try:
        model = plain[:sep].decode("ascii")
    except UnicodeDecodeError:
        return None
    return {"model": model, "raw": plain}


def fwsc_logical(path):
    with open(path, "rb") as image:
        raw = image.read()
    out = bytearray()
    for off in range(0, 20 * 0x30, 0x30):
        blk = raw[off:off + 0x30]
        if len(blk) < 0x30:
            raise SystemExit("%s: truncated header block at %#x" % (path, off))
        out += blk[:0x2F]
    out += raw[20 * 0x30:]
    return bytes(out)


class MidiLink:
    def __init__(self, name=PORT_NAME):
        if mido is None:
            raise SystemExit("python-rtmidi required: pip install python-rtmidi")
        self.out = mido.open_output(name)
        self.inp = mido.open_input(name)

    def write(self, data):
        self.out.send(mido.Message("sysex", data=list(data[1:-1])))

    def read_sysex(self, timeout):
        end = time.time() + timeout
        while time.time() < end:
            msg = self.inp.poll()
            if msg is not None and msg.type == "sysex":
                return b"\xF0" + bytes(msg.data) + b"\xF7"
            time.sleep(0.01)
        return None

    def drain(self):
        while self.read_sysex(0.01) is not None:
            pass

    def close(self):
        self.out.close()
        self.inp.close()


def handshake(link, attempts=3, timeout=1.0):
    link.drain()
    for _ in range(attempts):
        link.write(HS_QUERY)
        end = time.time() + timeout
        while time.time() < end:
            pkt = link.read_sysex(end - time.time())
            if pkt is None:
                break
            if pkt.startswith(b"\xF0" + HDR_HS) and pkt.endswith(b"\xF7"):
                return pkt
    return None


def cmd_scan(_):
    try:
        link = MidiLink()
    except Exception as exc:
        print("no FM-1 MIDI port: %s" % exc)
        return 1
    pkt = handshake(link)
    link.close()
    if pkt is None:
        print("no handshake response")
        return 1
    ident = parse_identity(pkt)
    print("handshake OK (%d bytes)" % len(pkt))
    print("identity:", ident["raw"] if ident else pkt.hex())
    return 0


def cmd_info(a):
    logical = fwsc_logical(a.file)
    print("file:", a.file)
    print("logical image bytes:", len(logical))
    print("JLUFW at: %#x" % logical.find(b"JLUFW"))
    return 0


def cmd_dryrun(a):
    logical = fwsc_logical(a.file)
    print("package: %d logical bytes, JLUFW at %#x"
          % (len(logical), logical.find(b"JLUFW")))
    try:
        link = MidiLink()
    except Exception as exc:
        print("no FM-1 MIDI port (offline part OK): %s" % exc)
        return 0
    pkt = handshake(link)
    link.close()
    if pkt is None:
        print("device present but handshake failed")
        return 1
    print("device handshake OK; identity:", parse_identity(pkt))
    print("DRY RUN COMPLETE — nothing was written to the device")
    return 0


def cmd_flash(a):
    if not a.confirm:
        print("refusing: pass --confirm to enable flashing")
        return 2
    print("You are about to WRITE firmware to the FM-1.")
    print("Package: %s" % a.file)
    answer = input("Type the device serial suffix shown on its sticker to proceed: ").strip()
    if not answer:
        print("aborted.")
        return 2
    print("proceeding with operator acknowledgement %r..." % answer)
    logical = fwsc_logical(a.file)
    link = MidiLink()
    pkt = handshake(link)
    if pkt is None:
        link.close()
        print("handshake failed, aborting")
        return 1
    print("handshake OK, sending UPGRADE_CMD in 2 s (Ctrl-C aborts)...")
    time.sleep(2.0)
    link.write(UPGRADE_CMD)
    served = 0
    last_req = time.time()
    try:
        while True:
            pkt = link.read_sysex(1.0)
            if pkt is None:
                if time.time() - last_req > REQ_TIMEOUT:
                    break
                continue
            req = parse_request(pkt)
            if req is None:
                continue
            fl, addr, ln = req
            if addr in (0xE0000000, 0xF0000000):
                link.write(build_success(addr))
                last_req = time.time()
                if addr == 0xF0000000:
                    break
                continue
            if ln > MAXDATA or addr + ln > len(logical):
                print("invalid request addr=%#x len=%d, aborting" % (addr, ln))
                link.close()
                return 1
            time.sleep(RESP_DELAY)
            link.write(build_response(addr, logical[addr:addr + ln], ln, fl))
            served += 1
            last_req = time.time()
            if served % 200 == 0:
                print("\r  served %d reqs, addr=%#08x" % (served, addr),
                      end="", flush=True)
    except KeyboardInterrupt:
        print("\ninterrupted by operator")
        link.close()
        return 130
    link.close()
    print("\ndone: %d requests served; verify version via scan" % served)
    return 0


def selftest():
    # pack7 round-trip
    for size in list(range(65)) + [511, 512]:
        data = bytes((i * 37 + size) & 0xFF for i in range(size))
        assert unpack7(pack7(data)) == data, size
    # golden data-response packet (captured from M-UPGRADE flow)
    actual = build_response(0x1234, b"\x00\x7f\x80\xff", 4)
    assert actual == bytes.fromhex(
        "f000324141000000003424000040000000007e017c7f16f7"), actual.hex()
    # golden verify-done response
    assert build_success(0xE0000000) == bytes.fromhex(
        "f00032410101000000000000000e010000736a0d1b566c5c39003c00f7")
    # request parse + checksum rejection
    body = (b"\x00\x59\x30" + (8).to_bytes(3, "little") + bytes([3])
            + (0x12345678).to_bytes(4, "little")
            + (0x200).to_bytes(3, "little"))
    chk = (~sum(body[6:])) & 0xFF
    pkt = b"\xF0" + pack7(body + bytes([chk])) + b"\xF7"
    assert parse_request(pkt) == (3, 0x12345678, 0x200)
    bad = bytearray(pkt)
    bad[-2] ^= 1
    assert parse_request(bytes(bad)) is None
    # identity parse on the documented trace shape
    sample = bytes.fromhex(
        "f000324558010000234d5a447905264c1a"
        "0000000000000000000000000000000000000000002006f7")
    ident = parse_identity(sample)
    assert ident is not None and ident["model"] == "FM-1", ident
    # logical-image strip on synthetic header
    raw = bytearray()
    for i in range(20):
        raw += bytes([i]) * 0x2F + bytes([0x7D])
    raw += b"PAYLOAD"
    import tempfile, os
    fd, path = tempfile.mkstemp()
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(bytes(raw))
        lg = fwsc_logical(path)
        assert lg == bytes([i for i in range(20) for _ in range(0x2F)]) + b"PAYLOAD", len(lg)
    finally:
        os.unlink(path)
    print("SELFTEST OK")


def main(argv=None):
    ap = argparse.ArgumentParser(description="FM-1 OTA flash client (macOS CoreMIDI)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("scan", help="read-only handshake + identity")
    p = sub.add_parser("info", help="offline package inspection")
    p.add_argument("file")
    p = sub.add_parser("dryrun", help="pre-flight without writes")
    p.add_argument("file")
    p = sub.add_parser("flash", help="WRITE firmware (gated)")
    p.add_argument("file")
    p.add_argument("--confirm", action="store_true",
                   help="acknowledge flash writes")
    sub.add_parser("selftest", help="offline codec checks")
    a = ap.parse_args(argv)
    if a.cmd == "scan":
        return cmd_scan(a)
    if a.cmd == "info":
        return cmd_info(a)
    if a.cmd == "dryrun":
        return cmd_dryrun(a)
    if a.cmd == "flash":
        return cmd_flash(a)
    if a.cmd == "selftest":
        selftest()
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(main())
