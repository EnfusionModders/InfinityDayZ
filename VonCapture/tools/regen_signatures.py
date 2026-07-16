# regen_signatures.py -- run inside IDA (File > Script file...) on DayZServer_x64.exe
#
# Regenerates the VonCapture byte signature for the VoN audio-relay function after
# a DayZ update shifts it. Anchors on the STABLE VoN packet magic 0xC1C1A027 (a
# protocol constant), finds the receive dispatcher that compares against it, then
# follows a call whose target reads the frame's blockSize at [rdx+9] -- that's the
# audio relay VonCapture detours.
#
# Output: the relay address + a best-effort IDA-style pattern to paste into
# VonCapture/Signatures.h (VON_RELAY_RETAIL). Verify the pattern is unique
# (Search > sequence of bytes) before shipping; keep structural displacements
# like the [rdx+9] '09' un-wildcarded if the auto-pattern wildcarded them.

import ida_bytes, ida_ua, ida_funcs, idautils, idc

MAGIC = 0xC1C1A027                     # VoN datagram tag (LE bytes: 27 A0 C1 C1)
MAGIC_BYTES = "27 A0 C1 C1"

def find_immediates(pattern):
    hits, ea = [], idc.get_inf_attr(idc.INF_MIN_EA)
    end = idc.get_inf_attr(idc.INF_MAX_EA)
    while True:
        ea = ida_bytes.find_bytes(pattern, ea + 1) if hasattr(ida_bytes, "find_bytes") \
             else idc.find_binary(ea + 1, idc.SEARCH_DOWN, pattern)
        if ea == idc.BADADDR or ea >= end:
            break
        hits.append(ea)
    return hits

def func_of(ea):
    f = ida_funcs.get_func(ea)
    return f.start_ea if f else idc.BADADDR

def reads_blocksize_at_rdx9(func_ea, scan=0x40):
    """True if the func reads a word at [rdx+9] near its start (movzx r?, word[rdx+9])."""
    ea = func_ea
    insn = ida_ua.insn_t()
    while ea < func_ea + scan:
        ln = ida_ua.decode_insn(insn, ea)
        if ln <= 0:
            ea += 1; continue
        # movzx (0F B7) with a memory operand [rdx + 0x09]
        mnem = idc.print_insn_mnem(ea).lower()
        if mnem in ("movzx", "mov", "movsx"):
            for op in insn.ops:
                if op.type == ida_ua.o_displ and (op.phrase == 1 or op.reg == 1) and op.addr == 9:
                    # reg 1 == rdx in IDA's x64 reg numbering for base
                    return True
            # fallback: raw byte check for '... 09' disp on an [rdx+..] form
        ea += ln
    return False

def make_pattern(ea, max_bytes=44):
    """Best-effort IDA pattern: keep opcodes/modrm, wildcard imm/disp/rel operand bytes."""
    parts, cur = [], ea
    insn = ida_ua.insn_t()
    while cur < ea + max_bytes:
        ln = ida_ua.decode_insn(insn, cur)
        if ln <= 0:
            parts.append("%02X" % ida_bytes.get_byte(cur)); cur += 1; continue
        wild = [False] * ln
        for op in insn.ops:
            if op.type == ida_ua.o_void:
                break
            if op.type in (ida_ua.o_imm, ida_ua.o_mem, ida_ua.o_displ, ida_ua.o_near, ida_ua.o_far):
                for i in range(op.offb, ln):
                    wild[i] = True
        for i in range(ln):
            parts.append("?" if wild[i] else "%02X" % ida_bytes.get_byte(cur + i))
        cur += ln
    return " ".join(parts)

def main():
    print("=" * 72)
    print("VonCapture signature regen — anchoring on VoN magic 0x%08X" % MAGIC)
    hits = find_immediates(MAGIC_BYTES)
    print("Found %d occurrence(s) of the magic in the image." % len(hits))

    dispatcher = idc.BADADDR
    for h in hits:
        f = func_of(h)
        if f != idc.BADADDR and idc.print_insn_mnem(h).lower() == "cmp":
            dispatcher = f
            print("  dispatcher (cmp vs magic) @ 0x%X  (magic ref @ 0x%X)" % (f, h))
            break
    if dispatcher == idc.BADADDR and hits:
        dispatcher = func_of(hits[0])
        print("  [fallback] using function of first magic ref @ 0x%X" % dispatcher)
    if dispatcher == idc.BADADDR:
        print("!! Could not locate the dispatcher. Magic constant may have changed.")
        return

    # Enumerate call targets in the dispatcher; pick the one reading [rdx+9].
    relay = idc.BADADDR
    for ins_ea in idautils.FuncItems(dispatcher):
        if idc.print_insn_mnem(ins_ea).lower() == "call":
            tgt = idc.get_operand_value(ins_ea, 0)
            if tgt and ida_funcs.get_func(tgt) and reads_blocksize_at_rdx9(tgt):
                relay = ida_funcs.get_func(tgt).start_ea
                print("  VoN audio relay @ 0x%X  (called from dispatcher @ 0x%X)" % (relay, ins_ea))
                break

    if relay == idc.BADADDR:
        print("!! Could not identify the relay by the [rdx+9] blockSize read.")
        print("   Inspect the dispatcher's subtype switch manually; the audio case")
        print("   (subtype 7) calls the relay.  dispatcher @ 0x%X" % dispatcher)
        return

    raw = " ".join("%02X" % ida_bytes.get_byte(relay + i) for i in range(44))
    print("-" * 72)
    print("relay first 44 bytes:\n  %s" % raw)
    print("-" * 72)
    print("Suggested VON_RELAY_RETAIL pattern (verify uniqueness; keep the")
    print("'0F B7 ?? 09' blockSize displacement and the 'cmp ..,14' un-wildcarded):")
    print("  %s" % make_pattern(relay))
    print("=" * 72)

main()
