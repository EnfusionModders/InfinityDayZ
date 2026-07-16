# VonCapture — server-side per-player VoN (voice) capture

An [InfinityDayZ](https://github.com/EnfusionModders/InfinityDayZ) plugin that
records every player's Voice-over-Network (VoN) audio to disk on the **dedicated
server**, for moderation. **No client changes** — it taps voice the server
already relays. Each player's audio is written per-speaker and converted offline
to standard `.opus` files you can play in VLC / any browser.

---

## How it works

The DayZ server does **not** decode voice — it *relays* encoded Opus frames
between clients (class `VoNSystemRetranslate`). VonCapture detours the server's
VoN audio-relay function and copies each frame's encoded Opus payload + the
speaker's id to a `.vcap` session file, then calls the original so voice keeps
flowing normally.

```
speaking client --Opus--> [server VoN relay  sub_1409AA1E0]
                                   |  (VonCapture detour taps here)
                                   +--> relay to listeners  (unchanged)
                                   +--> append {t, speakerId, Opus} to  Plugins/VonCapture/von_<epoch>.vcap
                                                    |
                                     tools/vcap_to_ogg.py  (offline)
                                                    |
                                        speaker_<id>.opus  (+ timeline.csv)
```

- **Codec:** Opus, mono, 8 kHz default, 20 ms frames (verified from the server binary).
- **The server never decodes**, so captured payloads are the *original* client
  Opus packets — bit-for-bit what the speaker sent.
- The tap runs under a structured-exception guard: a malformed frame can never
  crash the server.

---

## Install (dedicated server)

1. Install/locate your DayZ **dedicated server**.
2. Copy **`hid.dll`** into the **server root** (next to `DayZServer_x64.exe`).
3. Create a **`Plugins`** folder in the server root if it doesn't exist.
4. Copy **`InfinityHost.dll`** and **`VonCapture.dll`** into `Plugins/`.
5. Start the server. On load you'll see:
   ```
   [VonCapture] VoN relay resolved @ 0x...
   [VonCapture] capturing VoN audio to Plugins\VonCapture\von_<epoch>.vcap
   [VonCapture] active — per-player Opus frames are being captured.
   ```
   (Add `-debugprint` to the server args for verbose Infinity logging.)

A ready bundle is staged in `_CompiledBuild/` (`hid.dll` + `Plugins/InfinityHost.dll`
+ `Plugins/VonCapture.dll`).

> VonCapture is a passive native detour. It registers **no** Enforce script
> proto functions, so it needs **no** `-serverMod` / `@` mod — unlike the Example
> plugin.

---

## Getting the audio

Voice is captured to `Plugins/VonCapture/von_<epoch>.vcap`. Convert it offline
(no dependencies — pure Python 3):

```
python tools/vcap_to_ogg.py Plugins/VonCapture/von_1699999999.vcap
```

Produces `von_1699999999_out/`:
- `speaker_<id>.opus` — one playable Ogg-Opus file per speaker
- `timeline.csv` — speaker id, file, first/last timestamp, frame count, duration

Options:
- `-o OUTDIR` — output directory
- `--split-gap MS` — start a new file after a silence gap longer than `MS`
  (e.g. `--split-gap 2000` = one file per utterance-burst)

`<id>` is the VoN **speaker channel id**. Mapping it to SteamID/name is Phase 2
(see below); for now correlate by the time the player was speaking.

---

## Build from source

Requires Visual Studio 2022 (v143, C++20). From a shell:

```
build.bat
```

or directly:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  VonCapture\VonCapture.vcxproj -p:Configuration=Release -p:Platform=x64 ^
  -p:SolutionDir=%CD%\
```

Output: `x64\Release\Plugins\VonCapture.dll` (and its `InfinityHost.dll` dependency).
To edit in the IDE, open `Infinity.sln` and **Add > Existing Project > VonCapture.vcxproj**.

---

## Keeping it working across DayZ updates

VonCapture locates the relay by **byte signature** (`Signatures.h`), not a
hardcoded address, so it survives ASLR and minor recompiles. A **major** DayZ
update can still shift the function enough to break the pattern — you'll see:

```
[VonCapture] VoN relay pattern NOT found. Server build may differ ...
```

To regenerate the signature against the new `DayZServer_x64.exe`:

1. Open the new server exe in IDA (with the ida-pro-mcp setup, or standalone).
2. Run `tools/regen_signatures.py` (IDA: File > Script file...). It anchors on
   the stable VoN packet magic **`0xC1C1A027`**, finds the receive dispatcher,
   follows its subtype-7 branch to the audio relay, and prints a fresh IDA-style
   pattern.
3. Paste the new pattern into `VonCapture/Signatures.h` (`VON_RELAY_RETAIL`) and
   rebuild.

---

## Verification status

- ✅ Plugin **compiles** (VS2022, Release x64) and exports `?OnPluginLoad@@YAXXZ`
  exactly as InfinityHost's loader expects; imports `InfinityHost.dll`.
- ✅ Relay signature generated from and matched against the analyzed
  `DayZServer_x64.exe`; calling convention (rcx=vonSystem, rdx=frameBody,
  r8d=speakerId) confirmed from the prologue.
- ✅ Offline muxer **self-tested** (`vcap_to_ogg.py --selftest`): produces
  well-formed Ogg-Opus (CRC, segment lacing, packet reassembly, granule verified).
- ⏳ **Runtime capture** needs a live server with speaking players — see below.

### Runtime test procedure (on your server)
1. Install per above; start the server with `-debugprint`.
2. Confirm the `[VonCapture] VoN relay resolved @ ...` line appears.
3. Join with a client (or two) and speak on direct/radio.
4. Stop the server; you should have a non-trivial
   `Plugins/VonCapture/von_<epoch>.vcap`.
5. Run `vcap_to_ogg.py` on it and play a `speaker_<id>.opus` in VLC — you should
   hear the captured voice.

---

## Roadmap (Phase 2)

- **Identity mapping:** hook the connect path to build speakerId → SteamID/name,
  emit alongside `timeline.csv`.
- **Explicit start/stop:** hook the dispatcher (`sub_1409AB770` cases 1/4) to
  segment files precisely by utterance instead of by gap heuristic.
- **In-plugin Ogg-Opus:** write playable `.opus` directly (skip the offline step).
- **Config:** `voncapture.cfg` — enable/disable, per-player filters, output dir.

## Files

```
VonCapture/
  PluginMain.cpp     OnPluginLoad entrypoint
  VonCapture.cpp/.h  detour + .vcap writer
  Signatures.h       byte patterns for the VoN relay/dispatcher
  dllmain.cpp        DLL entry (flush on unload)
  VonCapture.vcxproj MSVC project (v143, x64, C++20)
  detours.h          MS Detours header (from Example)
  libraries/lib/     detours.lib
  tools/
    vcap_to_ogg.py       offline .vcap -> per-speaker Ogg-Opus
    regen_signatures.py  IDA script to regenerate the relay pattern on updates
```
