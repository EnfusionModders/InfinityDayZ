#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
//  VonCapture — server-side per-player VoN (voice) capture-to-file.
//
//  Detours the DayZ server VoN audio relay and writes every player's encoded
//  Opus voice frames, tagged with the speaker id, to a length-delimited
//  .vcap session file. Decode/mux to playable audio offline with
//  tools/vcap_to_ogg.py.
// ---------------------------------------------------------------------------

namespace VonCapture {

    // .vcap container constants (little-endian on disk).
    constexpr char     FILE_MAGIC[4]      = { 'V', 'C', 'A', 'P' };
    constexpr uint16_t FILE_VERSION       = 2;     // v2 adds identity records
    constexpr uint16_t FILE_HEADER_SIZE   = 16;
    constexpr uint32_t DEFAULT_SAMPLE_RATE = 8000; // VoN Opus default (negotiable)
    constexpr uint8_t  CHANNELS           = 1;     // mono
    constexpr uint8_t  REC_TYPE_AUDIO     = 1;
    constexpr uint8_t  REC_TYPE_IDENTITY  = 2;     // speakerId -> SteamID + name

    // Defensive bound: a single VoN frame's encoded payload is small
    // (8 kbps * 20 ms ~= 20 bytes; a few frames batched). Reject anything
    // implausible so a corrupt read can never blow up the writer.
    constexpr int MAX_PAYLOAD = 4096;

    // Called once from OnPluginLoad. Opens the session file and attaches the
    // relay detour. Returns false if the pattern could not be resolved.
    bool Init();

    // Appends one captured voice frame. Thread-safe, never throws.
    // payload points at the encoded Opus blob ([u8 len][opus]... concat).
    void WriteFrame(uint32_t speakerId, uint8_t subtype,
                    const uint8_t* payload, uint16_t len);

    // Appends a speakerId -> SteamID/name mapping (written on player connect).
    // steam64/name are safe local NUL-terminated buffers. Thread-safe.
    void WriteIdentity(uint32_t id, const char* steam64, const char* name);

    // Flushes and closes the session file (best-effort; DLL_PROCESS_DETACH).
    void Shutdown();

} // namespace VonCapture
