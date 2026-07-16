#!/usr/bin/env python3
"""
vcap_inspect.py -- diagnose a VonCapture .vcap file at the byte level.

Prints, per speaker, the raw payload bytes of the first frames and interprets
them two ways so we can tell how the Opus is really framed:

  (A) [u8 len][opus] blob  -- what OpusCodec::encode/decode use in the binary
  (B) whole payload = one raw Opus packet (no length prefix)

For each candidate Opus packet it decodes the RFC-6716 TOC byte (mode /
bandwidth / frame size / stereo / frame count) and flags whether it looks like
plausible VoN audio (SILK-NB/MB, 20 ms, mono). This tells us if our decoder is
splitting correctly or feeding the Opus decoder misaligned bytes.

Usage:  python vcap_inspect.py <session.vcap> [--frames 24] [--speaker ID]
"""

import argparse
import struct
import sys
from collections import Counter, defaultdict

# config -> (mode, bandwidth, frame_ms)
_TOC = {}
def _fill():
    modes = []
    def add(rng, mode, bw, sizes):
        for i, ms in zip(rng, sizes):
            _TOC[i] = (mode, bw, ms)
    add(range(0, 4),   "SILK",   "NB",  [10, 20, 40, 60])
    add(range(4, 8),   "SILK",   "MB",  [10, 20, 40, 60])
    add(range(8, 12),  "SILK",   "WB",  [10, 20, 40, 60])
    add(range(12, 14), "Hybrid", "SWB", [10, 20])
    add(range(14, 16), "Hybrid", "FB",  [10, 20])
    add(range(16, 20), "CELT",   "NB",  [2.5, 5, 10, 20])
    add(range(20, 24), "CELT",   "WB",  [2.5, 5, 10, 20])
    add(range(24, 28), "CELT",   "SWB", [2.5, 5, 10, 20])
    add(range(28, 32), "CELT",   "FB",  [2.5, 5, 10, 20])
_fill()

def decode_toc(b, nbytes):
    config = b >> 3
    stereo = (b >> 2) & 1
    code = b & 3
    mode, bw, ms = _TOC.get(config, ("?", "?", 0))
    if code == 0: nframes = 1
    elif code in (1, 2): nframes = 2
    else: nframes = "M(code3)"
    plausible = (mode in ("SILK", "Hybrid") and ms in (20, 40) and stereo == 0)
    return f"toc=0x{b:02X} {mode}/{bw} {ms}ms {'st' if stereo else 'mono'} frames={nframes}" \
           + ("  [plausible VoN]" if plausible else "  [<-- unexpected for VoN]")

def read_vcap(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"VCAP":
        raise SystemExit("not a .vcap file (bad magic)")
    version, hsz = struct.unpack_from("<HH", data, 4)
    rate, = struct.unpack_from("<I", data, 8)
    ch = data[12]
    off = hsz
    frames = []
    while off + 16 <= len(data):
        rt = data[off]; off += 1
        t, spk = struct.unpack_from("<QI", data, off); off += 12
        sub = data[off]; off += 1
        plen, = struct.unpack_from("<H", data, off); off += 2
        if off + plen > len(data): break
        frames.append((t, spk, sub, data[off:off+plen])); off += plen
    return version, rate, ch, frames

def walk_len8(payload):
    """Split as [u8 len][opus]... -> (packets, leftover_bytes, clean?)."""
    out, i, n = [], 0, len(payload)
    while i < n:
        L = payload[i]; i += 1
        if L == 0 or i + L > n:
            return out, n - i + 1, False
        out.append(payload[i:i+L]); i += L
    return out, 0, True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("vcap")
    ap.add_argument("--frames", type=int, default=20, help="frames to dump per speaker")
    ap.add_argument("--speaker", type=int, default=None, help="only this speaker id")
    args = ap.parse_args()

    version, rate, ch, frames = read_vcap(args.vcap)
    print(f"file: version={version} sampleRate={rate} channels={ch} totalAudioFrames={len(frames)}")

    by_spk = defaultdict(list)
    for t, spk, sub, pl in frames:
        by_spk[spk].append((t, sub, pl))
    print("speakers:", {s: len(v) for s, v in by_spk.items()})
    print()

    targets = [args.speaker] if args.speaker is not None else list(by_spk.keys())
    for spk in targets:
        recs = by_spk.get(spk, [])
        if not recs:
            continue
        print("=" * 78)
        print(f"SPEAKER {spk}  ({len(recs)} frames)")
        print("=" * 78)

        first_byte_hist = Counter()
        plen_hist = Counter()
        clean_len8 = 0
        toc_hist = Counter()
        for _, _, pl in recs:
            if pl:
                first_byte_hist[pl[0]] += 1
                plen_hist[len(pl)] += 1
                pkts, _, clean = walk_len8(pl)
                if clean and pkts:
                    clean_len8 += 1
                    for p in pkts:
                        if p:
                            toc_hist[p[0]] += 1

        print(f"payloadLen histogram : {dict(sorted(plen_hist.items()))}")
        print(f"payload[0] histogram : {dict(sorted(first_byte_hist.items()))}")
        print(f"[u8 len] clean parse : {clean_len8}/{len(recs)} frames "
              f"({100*clean_len8//max(1,len(recs))}%)")
        if toc_hist:
            print(f"TOC-of-2nd-byte hist : {dict(sorted(toc_hist.items()))}  "
                  f"(if [u8 len][opus] is right, these are Opus TOC bytes)")
        print()

        for k, (t, sub, pl) in enumerate(recs[:args.frames]):
            print(f"[{k:02d}] t={t}ms sub={sub} len={len(pl)}  hex={pl.hex()}")
            # Hypothesis A: [u8 len][opus]
            pkts, leftover, clean = walk_len8(pl)
            tag = "CLEAN" if clean else f"MISALIGNED(leftover~{leftover})"
            print(f"     A [u8 len][opus]: {len(pkts)} pkt(s) {tag}")
            for p in pkts[:3]:
                print(f"        len={len(p):3d}  {decode_toc(p[0], len(p)) if p else '(empty)'}")
            # Hypothesis B: whole payload = one raw opus packet
            if pl:
                print(f"     B whole=opus   : {decode_toc(pl[0], len(pl))}")
        print()

    print("-" * 78)
    print("READ ME:")
    print(" * If (A) parses CLEAN and the packet TOCs say SILK 20ms mono -> framing")
    print("   is correct; the bug is in the muxer, and we compare via a real decoder.")
    print(" * If (A) is MISALIGNED but (B)'s TOC looks like plausible VoN -> payload")
    print("   is a single raw Opus packet; fix = don't split on [u8 len].")
    print(" * If neither looks like Opus -> capture offset is off; we revisit the plugin.")

if __name__ == "__main__":
    main()
