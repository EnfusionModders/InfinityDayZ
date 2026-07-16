#!/usr/bin/env python3
"""
vcap_to_ogg.py  --  Offline decoder for VonCapture .vcap files.

VonCapture (the DayZ server Infinity plugin) records every player's raw,
still-encoded VoN Opus frames to a .vcap session file. The DayZ server never
decodes voice (it relays), so the payloads are standard RFC-6716 Opus packets
exactly as the speaking client produced them (mono, 8 kHz default, 20 ms CBR).

This tool groups frames by speaker and muxes each speaker's Opus packets into a
standard Ogg-Opus (.opus) file (RFC 7845) that plays in VLC / ffmpeg / browsers.
No decoding is required here -- we simply repackage the existing Opus packets
into an Ogg container -- so this has ZERO third-party dependencies.

.vcap format (little-endian):
  Header (16 bytes):  "VCAP" | u16 version | u16 headerSize | u32 sampleRate |
                      u8 channels | u8[3] reserved
  Record (repeated):  u8 recType(1=audio) | u64 t_ms | u32 speakerId |
                      u8 subtype | u16 payloadLen | u8[payloadLen] payload
  payload = concat of [u8 opusLen][opusLen bytes of one Opus packet]

Usage:
  python vcap_to_ogg.py session.vcap                 # -> session_out/speaker_<id>.opus (+ timeline.csv)
  python vcap_to_ogg.py session.vcap -o outdir
  python vcap_to_ogg.py session.vcap --split-gap 2000  # new file after >2s silence
  python vcap_to_ogg.py --selftest                   # validate the muxer end-to-end
"""

import argparse
import os
import struct
import sys

# --------------------------------------------------------------------------- #
#  Ogg container primitives
# --------------------------------------------------------------------------- #

def _make_crc_table():
    table = []
    for i in range(256):
        r = i << 24
        for _ in range(8):
            r = ((r << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if (r & 0x80000000) else (r << 1) & 0xFFFFFFFF
        table.append(r)
    return table

_CRC_TABLE = _make_crc_table()

def ogg_crc(data: bytes) -> int:
    """Ogg's CRC-32 (poly 0x04C11DB7, no reflection, init 0, no final xor)."""
    crc = 0
    for b in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ _CRC_TABLE[((crc >> 24) & 0xFF) ^ b]
    return crc

def build_page(serial, page_seq, granule, packets, bos=False, eos=False, cont=False):
    """Assemble one Ogg page from a list of complete packets (each <= 255*255)."""
    seg_table = bytearray()
    body = bytearray()
    for pkt in packets:
        n = len(pkt)
        while n >= 255:
            seg_table.append(255)
            n -= 255
        seg_table.append(n)          # trailing lacing value (may be 0)
        body += pkt
    if len(seg_table) > 255:
        raise ValueError("too many segments for one page")

    header_type = (0x01 if cont else 0) | (0x02 if bos else 0) | (0x04 if eos else 0)
    page = bytearray()
    page += b"OggS"
    page += bytes([0])                       # stream structure version
    page += bytes([header_type])
    page += struct.pack("<q", granule)       # -1 => 0xFFFFFFFFFFFFFFFF
    page += struct.pack("<I", serial & 0xFFFFFFFF)
    page += struct.pack("<I", page_seq & 0xFFFFFFFF)
    page += struct.pack("<I", 0)             # CRC placeholder
    page += bytes([len(seg_table)])
    page += bytes(seg_table)
    page += bytes(body)
    crc = ogg_crc(bytes(page))
    page[22:26] = struct.pack("<I", crc)
    return bytes(page)

def opus_head(channels=1, pre_skip=0, input_rate=8000):
    return (b"OpusHead"
            + bytes([1, channels])
            + struct.pack("<H", pre_skip)
            + struct.pack("<I", input_rate)
            + struct.pack("<h", 0)           # output gain
            + bytes([0]))                    # channel mapping family

def opus_tags(vendor=b"VonCapture"):
    return (b"OpusTags"
            + struct.pack("<I", len(vendor)) + vendor
            + struct.pack("<I", 0))          # 0 user comments

# --------------------------------------------------------------------------- #
#  Opus packet duration (RFC 6716 TOC) -> granule samples @ 48 kHz
# --------------------------------------------------------------------------- #

# frame samples @48k per config (0..31)
_SILK   = [480, 960, 1920, 2880]     # 10/20/40/60 ms
_HYBRID = [480, 960]                 # 10/20 ms
_CELT   = [120, 240, 480, 960]       # 2.5/5/10/20 ms
_FRAME_48K = (_SILK*3 + _HYBRID*2 + _CELT*4)   # 12 + 4 + 16 = 32 entries

def opus_packet_samples48(pkt: bytes) -> int:
    if not pkt:
        return 0
    toc = pkt[0]
    config = toc >> 3
    code = toc & 0x03
    if code == 0:
        nframes = 1
    elif code in (1, 2):
        nframes = 2
    else:  # code 3
        nframes = (pkt[1] & 0x3F) if len(pkt) > 1 else 1
    return _FRAME_48K[config] * max(1, nframes)

# --------------------------------------------------------------------------- #
#  .vcap reader
# --------------------------------------------------------------------------- #

class VcapError(Exception):
    pass

def read_vcap(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 16 or data[:4] != b"VCAP":
        raise VcapError("not a .vcap file (bad magic)")
    version, hsz = struct.unpack_from("<HH", data, 4)
    sample_rate, = struct.unpack_from("<I", data, 8)
    channels = data[12]
    n = len(data)
    off = hsz
    frames = []           # (t_ms, speakerId, subtype, payload bytes)
    identities = {}       # id -> (steam64, name)   (record type 2, .vcap v2+)
    while off < n:
        rec_type = data[off]; off += 1
        if off + 8 > n:
            break
        t_ms, = struct.unpack_from("<Q", data, off); off += 8
        if rec_type == 1:                              # audio frame
            if off + 7 > n:
                break
            speaker, = struct.unpack_from("<I", data, off); off += 4
            subtype = data[off]; off += 1
            plen, = struct.unpack_from("<H", data, off); off += 2
            if off + plen > n:
                break                                  # truncated tail -> stop cleanly
            frames.append((t_ms, speaker, subtype, data[off:off + plen])); off += plen
        elif rec_type == 2:                            # identity mapping
            if off + 5 > n:
                break
            pid, = struct.unpack_from("<I", data, off); off += 4
            sl = data[off]; off += 1
            if off + sl + 1 > n:
                break
            steam = data[off:off + sl].decode("utf-8", "replace"); off += sl
            nl = data[off]; off += 1
            if off + nl > n:
                break
            name = data[off:off + nl].decode("utf-8", "replace"); off += nl
            identities[pid] = (steam, name)
        else:
            break                                      # unknown record -> stop
    return {"version": version, "sample_rate": sample_rate,
            "channels": channels, "frames": frames, "identities": identities}


def safe_label(spk, steam, name):
    """Build a filesystem-safe label for a speaker from its identity (if known)."""
    if name or steam:
        base = (name or "").strip() or f"id{spk}"
        clean = "".join(c if (c.isalnum() or c in "._-") else "_" for c in base)[:40]
        clean = clean.strip("_") or f"id{spk}"
        return f"{clean}_{steam}" if steam else clean
    return str(spk)

def extract_opus_packets(payload):
    """Extract raw Opus packets from a VoN network payload.

    Framing verified against DayZServer_x64.exe + real captures: the payload is
    a batch of sub-frames, each

        [u16 LE blobLen] [ codec-blob ]

    where the codec-blob is OpusCodec's own output, one or more

        [u8 opusLen] [opus packet]

    (blobLen == opusLen + 1 for the common one-frame case). A typical VoN
    message batches ~13 of these 20 ms sub-frames.
    """
    out = []
    i, n = 0, len(payload)
    while i + 2 <= n:
        blob_len = payload[i] | (payload[i + 1] << 8); i += 2
        if blob_len == 0 or i + blob_len > n:
            break                      # truncated tail (e.g. final message) -> stop
        blob = payload[i:i + blob_len]; i += blob_len
        j, m = 0, len(blob)
        while j < m:                   # codec-blob may hold >1 opus frame
            L = blob[j]; j += 1
            if L == 0 or j + L > m:
                break
            out.append(blob[j:j + L]); j += L
    return out

# --------------------------------------------------------------------------- #
#  Muxer
# --------------------------------------------------------------------------- #

def write_ogg_opus(path, opus_packets, channels=1, input_rate=8000, packets_per_page=50):
    serial = (hash(path) & 0x7FFFFFFF) or 1
    with open(path, "wb") as f:
        seq = 0
        # BOS: OpusHead on its own page
        f.write(build_page(serial, seq, 0, [opus_head(channels, 0, input_rate)], bos=True)); seq += 1
        # OpusTags on its own page
        f.write(build_page(serial, seq, 0, [opus_tags()])); seq += 1
        # audio pages
        granule = 0
        i = 0
        total = len(opus_packets)
        while i < total:
            batch = opus_packets[i:i + packets_per_page]
            i += len(batch)
            for pkt in batch:
                granule += opus_packet_samples48(pkt)
            eos = (i >= total)
            f.write(build_page(serial, seq, granule, batch, eos=eos)); seq += 1
        if total == 0:  # emit an empty EOS page so the stream is well-formed
            f.write(build_page(serial, seq, 0, [], eos=True))
    return granule

def convert(vcap_path, outdir, split_gap_ms=None):
    info = read_vcap(vcap_path)
    frames = info["frames"]
    rate = info["sample_rate"] or 8000
    if not frames:
        print("no audio frames found in", vcap_path)
        return
    os.makedirs(outdir, exist_ok=True)

    ids = info.get("identities", {})
    matched = sum(1 for s in set(f[1] for f in frames) if s in ids)
    print(f"identities: {len(ids)} recorded, {matched} match a captured speaker")

    # group by speaker, preserving arrival order
    by_speaker = {}
    for t_ms, spk, subtype, payload in frames:
        by_speaker.setdefault(spk, []).append((t_ms, payload))

    timeline = []  # (speaker, steam, name, file, first_ms, last_ms, packets)
    for spk, recs in sorted(by_speaker.items()):
        steam, name = ids.get(spk, ("", ""))
        label = safe_label(spk, steam, name)

        segments = [[]]      # list of segments; each is list of (t_ms, packets)
        last_t = None
        for t_ms, payload in recs:
            if split_gap_ms and last_t is not None and (t_ms - last_t) > split_gap_ms:
                segments.append([])
            segments[-1].append((t_ms, extract_opus_packets(payload)))
            last_t = t_ms

        for idx, seg in enumerate(segments):
            pkts = [p for _, plist in seg for p in plist]
            if not pkts:
                continue
            suffix = "" if len(segments) == 1 else f"_seg{idx:02d}"
            fname = f"speaker_{label}{suffix}.opus"
            fpath = os.path.join(outdir, fname)
            # input_sample_rate in OpusHead is informational (RFC 7845); Opus
            # always decodes at 48 kHz. VoN is SILK-WB, so 48000 is safe/canonical.
            write_ogg_opus(fpath, pkts, channels=info["channels"] or 1, input_rate=48000)
            first_ms = seg[0][0]
            last_ms = seg[-1][0]
            timeline.append((spk, steam, name, fname, first_ms, last_ms, len(pkts)))
            who = f"{name} [{steam}]" if (name or steam) else f"id {spk}"
            print(f"  {who}: {len(pkts):5d} frames  "
                  f"({(last_ms-first_ms)/1000:.1f}s span)  -> {fname}")

    csv_path = os.path.join(outdir, "timeline.csv")
    with open(csv_path, "w", encoding="utf-8") as c:
        c.write("speaker_id,steam64,name,file,first_ms,last_ms,frames,approx_seconds\n")
        for spk, steam, name, fname, a, b, cnt in timeline:
            nm = (name or "").replace(",", " ")
            c.write(f"{spk},{steam},{nm},{fname},{a},{b},{cnt},{cnt*0.02:.2f}\n")
    print(f"wrote {len(timeline)} file(s) + timeline.csv to {outdir}")

# --------------------------------------------------------------------------- #
#  Self-test: mux synthetic frames, then re-parse and validate the container
# --------------------------------------------------------------------------- #

def _parse_ogg(data):
    """Minimal Ogg parser -> list of pages; validates CRC + capture pattern."""
    pages = []
    off = 0
    while off < len(data):
        if data[off:off+4] != b"OggS":
            raise AssertionError(f"bad capture pattern at {off}")
        header_type = data[off+5]
        granule, = struct.unpack_from("<q", data, off+6)
        serial, = struct.unpack_from("<I", data, off+14)
        seq, = struct.unpack_from("<I", data, off+18)
        stored_crc, = struct.unpack_from("<I", data, off+22)
        nsegs = data[off+26]
        seg_table = data[off+27:off+27+nsegs]
        body_len = sum(seg_table)
        page_len = 27 + nsegs + body_len
        page = bytearray(data[off:off+page_len])
        page[22:26] = b"\x00\x00\x00\x00"
        assert ogg_crc(bytes(page)) == stored_crc, f"CRC mismatch page seq {seq}"
        # reassemble packets from lacing
        body = data[off+27+nsegs:off+page_len]
        packets, cur, bo = [], 0, 0
        for lv in seg_table:
            cur += lv
            if lv < 255:
                packets.append(body[bo:bo+cur]); bo += cur; cur = 0
        pages.append({"header_type": header_type, "granule": granule,
                      "seq": seq, "packets": packets})
        off += page_len
    return pages

def selftest():
    print("[selftest] building synthetic .vcap ...")
    # Synthesize Opus-looking packets. TOC 0x08 = config 1 (SILK NB 20ms) code 0
    # -> 960 samples @48k, exactly the VoN default. Content is arbitrary (the Ogg
    # container does not interpret it; real captures carry real Opus).
    def fake_frame(nbytes):
        return bytes([0x08]) + bytes((7 * i + 3) & 0xFF for i in range(nbytes - 1))

    tmp = os.path.join(os.environ.get("TEMP", "."), "_voncap_selftest.vcap")
    with open(tmp, "wb") as f:
        f.write(b"VCAP" + struct.pack("<HH", 2, 16) + struct.pack("<I", 8000) + bytes([1, 0, 0, 0]))
        # identity record (type 2) for speaker 1001 -> file named by player
        steam, nm = b"76561198000000001", b"TestPlayer"
        f.write(bytes([2]) + struct.pack("<QI", 0, 1001)
                + bytes([len(steam)]) + steam + bytes([len(nm)]) + nm)
        # two speakers, a few frames each; speaker 1001 has a 3s gap for split test
        plan = [(0, 1001), (20, 1001), (40, 1001), (5000, 1001),  # gap -> seg
                (10, 2002), (30, 2002)]
        for t, spk in plan:
            opus = fake_frame(20)                       # 20-byte opus packet (TOC 0x08)
            blob = bytes([len(opus)]) + opus            # [u8 opusLen][opus]  (codec blob)
            payload = struct.pack("<H", len(blob)) + blob   # [u16 blobLen][blob]  (VoN framing)
            rec = (bytes([1]) + struct.pack("<QI", t, spk) + bytes([7])
                   + struct.pack("<H", len(payload)) + payload)
            f.write(rec)

    info = read_vcap(tmp)
    assert len(info["frames"]) == 6, info["frames"]
    assert info["identities"].get(1001) == ("76561198000000001", "TestPlayer"), info["identities"]
    print(f"[selftest] parsed {len(info['frames'])} frames, "
          f"{len(info['identities'])} identity, rate={info['sample_rate']}")

    outdir = os.path.join(os.environ.get("TEMP", "."), "_voncap_selftest_out")
    for old in os.listdir(outdir) if os.path.isdir(outdir) else []:
        os.remove(os.path.join(outdir, old))     # clean stale files from prior runs
    convert(tmp, outdir, split_gap_ms=2000)

    names = [n for n in os.listdir(outdir) if n.endswith(".opus")]
    assert any(n.startswith("speaker_TestPlayer_76561198000000001") for n in names), names
    assert any(n == "speaker_2002.opus" for n in names), names   # no identity -> raw id

    # validate every produced .opus is a well-formed Ogg-Opus stream
    ok = 0
    for name in sorted(names):
        if not name.endswith(".opus"):
            continue
        with open(os.path.join(outdir, name), "rb") as f:
            data = f.read()
        pages = _parse_ogg(data)                       # asserts capture pattern + CRCs
        assert pages[0]["packets"][0].startswith(b"OpusHead"), name
        assert pages[1]["packets"][0].startswith(b"OpusTags"), name
        assert pages[-1]["header_type"] & 0x04, f"{name}: missing EOS"
        granules = [p["granule"] for p in pages[2:]]
        assert granules == sorted(granules), f"{name}: granule not monotonic"
        # each fake frame = 960 samples @48k; audio pages should sum to 960*Naudio
        naudio = sum(len(p["packets"]) for p in pages[2:])
        assert pages[-1]["granule"] == 960 * naudio, (name, pages[-1]["granule"], naudio)
        ok += 1
        print(f"[selftest]   {name}: {len(pages)} pages, "
              f"{naudio} audio frames, final granule {pages[-1]['granule']} — OK")
    # speaker 1001 split into 2 segments (gap), speaker 2002 single -> 3 files
    assert ok == 3, f"expected 3 output files, got {ok}"
    print("[selftest] PASS — Ogg-Opus container is well-formed (CRC, lacing, granule verified).")
    print("[selftest] Note: real .opus playback (VLC/ffmpeg) requires real captured")
    print("           Opus payloads; synthetic bytes here validate structure only.")
    return 0

# --------------------------------------------------------------------------- #

def main():
    ap = argparse.ArgumentParser(description="Convert VonCapture .vcap -> per-speaker Ogg-Opus")
    ap.add_argument("vcap", nargs="?", help="input .vcap file")
    ap.add_argument("-o", "--outdir", help="output directory (default: <vcap>_out)")
    ap.add_argument("--split-gap", type=int, default=None,
                    help="start a new file after a silence gap of this many ms")
    ap.add_argument("--selftest", action="store_true", help="run the built-in muxer self-test")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if not args.vcap:
        ap.print_help()
        return 2
    outdir = args.outdir or (os.path.splitext(args.vcap)[0] + "_out")
    print(f"decoding {args.vcap} -> {outdir}")
    convert(args.vcap, outdir, split_gap_ms=args.split_gap)
    return 0

if __name__ == "__main__":
    sys.exit(main())
