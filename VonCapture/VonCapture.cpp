#include <Windows.h>
#include <InfinityPlugin.h>

#include "detours.h"
#include "VonCapture.h"
#include "Signatures.h"

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <fstream>
#include <string>

using namespace Infinity;

namespace VonCapture {

    // ----- session state -------------------------------------------------
    static std::mutex   g_mtx;
    static std::ofstream g_file;
    static uint64_t     g_startTick   = 0;
    static uint32_t     g_sinceFlush  = 0;
    static uint64_t     g_frameCount  = 0;

    // ----- original relay pointer + detour --------------------------------
    //   sub_1409AA1E0(void* vonSystem /*rcx*/, uint8_t* body /*rdx*/,
    //                 uint32_t speakerId /*r8d*/)
    using FnVonRelay = void(__fastcall*)(void* vonSystem, uint8_t* body, uint32_t speakerId);
    static FnVonRelay f_VonRelay = nullptr;

    // POD-only tap so we can SEH-guard the raw reads without C++ unwind objects
    // in this frame. Any access violation on a malformed frame is swallowed —
    // capture must NEVER be able to crash the server.
    static void CaptureFrame(uint8_t* body, uint32_t speakerId) {
        __try {
            if (!body) return;
            const uint16_t blockSize = *reinterpret_cast<const uint16_t*>(body + 9);
            if (blockSize <= 24) return;                 // no audio payload
            const int encLen = static_cast<int>(blockSize) - 24;
            if (encLen <= 0 || encLen > MAX_PAYLOAD) return;
            const uint8_t  subtype = *(body + 8);        // 7 == audio
            const uint8_t* payload = body + 35;
            WriteFrame(speakerId, subtype, payload, static_cast<uint16_t>(encLen));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // corrupt/short frame — ignore, keep the server alive
        }
    }

    static void __fastcall Detour_VonRelay(void* vonSystem, uint8_t* body, uint32_t speakerId) {
        CaptureFrame(body, speakerId);
        f_VonRelay(vonSystem, body, speakerId);          // always relay normally
    }

    // ----- player-connect detour (speakerId -> SteamID/name) --------------
    //   sub_1406C88C0(u32 dpid, char* steam64, char flag, char* name, a5, netServer)
    //   dpid is the VoN channel id == the relay speakerId.
    using FnOnConnect = __int64(__fastcall*)(uint32_t dpid, char* steam64, char flag,
                                             char* name, __int64 a5, __int64 netServer);
    static FnOnConnect f_OnConnect = nullptr;

    static __int64 __fastcall Detour_OnConnect(uint32_t dpid, char* steam64, char flag,
                                               char* name, __int64 a5, __int64 netServer) {
        // Copy the engine's strings into safe local buffers under SEH FIRST, so a
        // bad pointer can't fault while holding the writer lock.
        char s[40] = { 0 }, n[48] = { 0 };
        __try {
            if (steam64) for (int i = 0; i < (int)sizeof(s) - 1 && steam64[i]; ++i) s[i] = steam64[i];
            if (name)    for (int i = 0; i < (int)sizeof(n) - 1 && name[i];    ++i) n[i] = name[i];
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { s[0] = 0; n[0] = 0; }

        WriteIdentity(dpid, s, n);                        // safe local buffers
        return f_OnConnect(dpid, steam64, flag, name, a5, netServer);
    }

    // ----- file writer ----------------------------------------------------
    static void WriteHeaderLocked() {
        const uint16_t ver   = FILE_VERSION;
        const uint16_t hsz   = FILE_HEADER_SIZE;
        const uint32_t rate  = DEFAULT_SAMPLE_RATE;
        const uint8_t  chans = CHANNELS;
        const uint8_t  pad[3] = { 0, 0, 0 };
        g_file.write(FILE_MAGIC, 4);
        g_file.write(reinterpret_cast<const char*>(&ver),   2);
        g_file.write(reinterpret_cast<const char*>(&hsz),   2);
        g_file.write(reinterpret_cast<const char*>(&rate),  4);
        g_file.write(reinterpret_cast<const char*>(&chans), 1);
        g_file.write(reinterpret_cast<const char*>(pad),    3);
    }

    void WriteFrame(uint32_t speakerId, uint8_t subtype,
                    const uint8_t* payload, uint16_t len) {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!g_file.is_open()) return;

        const uint8_t  recType = REC_TYPE_AUDIO;
        const uint64_t t_ms    = GetTickCount64() - g_startTick;

        g_file.write(reinterpret_cast<const char*>(&recType),   1);
        g_file.write(reinterpret_cast<const char*>(&t_ms),      8);
        g_file.write(reinterpret_cast<const char*>(&speakerId), 4);
        g_file.write(reinterpret_cast<const char*>(&subtype),   1);
        g_file.write(reinterpret_cast<const char*>(&len),       2);
        g_file.write(reinterpret_cast<const char*>(payload),    len);

        ++g_frameCount;
        if (++g_sinceFlush >= 50) { g_file.flush(); g_sinceFlush = 0; }
    }

    void WriteIdentity(uint32_t id, const char* steam64, const char* name) {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!g_file.is_open()) return;

        const uint8_t  recType = REC_TYPE_IDENTITY;
        const uint64_t t_ms    = GetTickCount64() - g_startTick;
        uint8_t sl = 0; while (sl < 39 && steam64[sl]) ++sl;
        uint8_t nl = 0; while (nl < 47 && name[nl])    ++nl;

        g_file.write(reinterpret_cast<const char*>(&recType), 1);
        g_file.write(reinterpret_cast<const char*>(&t_ms),    8);
        g_file.write(reinterpret_cast<const char*>(&id),      4);
        g_file.write(reinterpret_cast<const char*>(&sl),      1);
        g_file.write(steam64, sl);
        g_file.write(reinterpret_cast<const char*>(&nl),      1);
        g_file.write(name, nl);
        g_file.flush();   // identity records are rare + important

        Logging::Println("[VonCapture] identity: id=%u steam=%s name=%s", id, steam64, name);
    }

    // ----- detour attach (mirrors Example's PluginUtils::AttachDetour) -----
    static bool AttachRelayDetour() {
        const std::string& pattern = Logging::IsDiagBuild()
            ? Sig::VON_RELAY_DIAG
            : Sig::VON_RELAY_RETAIL;

        void* addr = Utils::FindPattern(pattern, GetModuleHandle(nullptr), 0);
        if (!addr) {
            Logging::Errorln("[VonCapture] VoN relay pattern NOT found. "
                             "Server build may differ; regenerate the signature "
                             "(tools/regen_signatures.py).");
            return false;
        }

        Logging::Println("[VonCapture] VoN relay resolved @ 0x%llX",
                         reinterpret_cast<unsigned long long>(addr));

        f_VonRelay = reinterpret_cast<FnVonRelay>(addr);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&reinterpret_cast<PVOID&>(f_VonRelay), Detour_VonRelay);
        const LONG err = DetourTransactionCommit();
        if (err != NO_ERROR) {
            Logging::Errorln("[VonCapture] DetourAttach failed (err=%ld)", err);
            return false;
        }
        return true;
    }

    // Best-effort: enables speakerId -> SteamID/name mapping. Capture works without it.
    static bool AttachConnectDetour() {
        void* addr = Utils::FindPattern(Sig::ON_CONNECT_RETAIL, GetModuleHandle(nullptr), 0);
        if (!addr) {
            Logging::Warnln("[VonCapture] connect pattern NOT found -> files will be named "
                            "by raw VoN id (no SteamID/name mapping).");
            return false;
        }
        f_OnConnect = reinterpret_cast<FnOnConnect>(addr);
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&reinterpret_cast<PVOID&>(f_OnConnect), Detour_OnConnect);
        if (DetourTransactionCommit() != NO_ERROR) {
            Logging::Warnln("[VonCapture] connect DetourAttach failed -> no identity mapping.");
            return false;
        }
        Logging::Println("[VonCapture] identity mapping active (connect hook @ 0x%llX)",
                         reinterpret_cast<unsigned long long>(addr));
        return true;
    }

    // ----- lifecycle ------------------------------------------------------
    bool Init() {
        CreateDirectoryA("Plugins", nullptr);
        CreateDirectoryA("Plugins\\VonCapture", nullptr);

        char path[MAX_PATH];
        std::snprintf(path, sizeof(path),
                      "Plugins\\VonCapture\\von_%llu.vcap",
                      static_cast<unsigned long long>(::time(nullptr)));

        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_file.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!g_file.is_open()) {
                Logging::Errorln("[VonCapture] could not open capture file: %s", path);
                return false;
            }
            WriteHeaderLocked();
            g_file.flush();
            g_startTick = GetTickCount64();
        }
        Logging::Println("[VonCapture] capturing VoN audio to %s", path);

        if (!AttachRelayDetour()) return false;
        AttachConnectDetour();   // best-effort; audio capture works regardless

        Logging::Println("[VonCapture] active — per-player Opus frames are being captured.");
        return true;
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_file.is_open()) {
            g_file.flush();
            g_file.close();
        }
    }

} // namespace VonCapture
