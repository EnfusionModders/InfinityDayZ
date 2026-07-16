#pragma once
#include <string>

// ---------------------------------------------------------------------------
//  VonCapture signatures
//
//  Byte patterns for locating the DayZ server VoN functions at runtime via
//  Infinity::Utils::FindPattern. Patterns are resilient to ASLR / minor
//  recompiles (operand displacements are wildcarded) but are tied to a given
//  DayZServer_x64.exe build. Regenerate with tools/regen_signatures.py if the
//  server updates and the pattern stops resolving.
//
//  Reversed from DayZServer_x64.exe (F:\DayZInstances\test-c5305f84), verified:
//    sub_1409AA1E0  VoN audio relay (subtype 7). __fastcall(void* vonSystem,
//                   uint8_t* frameBody /*rdx*/, uint32_t speakerId /*r8d*/).
//                   Prologue:
//                     44 89 44 24 18   mov [rsp+18], r8d   ; speakerId
//                     48 89 54 24 10   mov [rsp+10], rdx   ; frameBody
//                     48 89 4C 24 08   mov [rsp+08], rcx   ; vonSystem
//                     56 57 41 54 41 57                     ; push rsi/rdi/r12/r15
//                     48 83 EC 78      sub rsp, 78h
//                     44 0F B7 62 09   movzx r12d, word [rdx+9]  ; blockSize
//                     41 8B F8 / 4C 8B FA / 48 8B F1
//                     41 83 FC 14      cmp r12d, 14h        ; blockSize > 20 gate
//
//  On-wire received frame layout (verified):
//    +0  u32 magic 0xC1C1A027   +4  u32 speakerChannelId   +8  u8 subtype(7)
//    +9  u16 blockSize(>20)     +11 16B spatial hdr        +27 u32 routing word
//    +35 ... Opus payload, length = (u16@+9) - 24
// ---------------------------------------------------------------------------

namespace VonCapture {
namespace Sig {

    // The distinctive tail `44 0F B7 62 09 ... 41 83 FC 14`
    // (movzx r12d, word[rdx+9]; cmp r12d, 14h) makes this unique to the relay.
    inline std::string VON_RELAY_RETAIL =
        "44 89 44 24 ? 48 89 54 24 ? 48 89 4C 24 ? 56 57 41 54 41 57 "
        "48 83 EC ? 44 0F B7 62 09 41 8B F8 4C 8B FA 48 8B F1 41 83 FC 14";

    // DIAG (-diag) build not yet reversed. Falls back to RETAIL; regenerate
    // against the diag exe with tools/regen_signatures.py if you run -diag.
    inline std::string VON_RELAY_DIAG = VON_RELAY_RETAIL;

    // Phase 2: dispatcher (start/stop/audio demux) for explicit segmentation.
    //   sub_1409AB770 __fastcall(msgObj /*rcx*/, ?, vonSystem /*r8*/)
    inline std::string VON_DISPATCH_RETAIL =
        "48 89 5C 24 ? 57 48 83 EC 20 49 8B F8 48 8B D9 48 85 C9";

    // Player connect callback -> identity mapping (speakerId -> SteamID/name).
    //   sub_1406C88C0 __fastcall(u32 dpid /*ecx*/, char* steam64 /*rdx*/,
    //                            char flag /*r8b*/, char* name /*r9*/, ..., netServer)
    // dpid == the VoN channel id == the relay speakerId (verified: the connect
    // handler sub_140666A50 allocates this id, stores name+GUID with it, and
    // registers VoN via sub_1409A7AC0(vonSystem, id)). Same pattern the Infinity
    // ExamplePlugin uses for its connect hook.
    inline std::string ON_CONNECT_RETAIL =
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 49 8B F9 48 8B EA";

} // namespace Sig
} // namespace VonCapture
