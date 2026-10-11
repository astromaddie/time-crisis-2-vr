# Steam Frame architecture

## Target and build

The Frame flavor is a separate macOS-hosted ARM64 Android/OpenXR build of the existing Time Crisis II source. `tools/build_frame.py` enables `TCVR_STEAM_FRAME`, targets `arm64-v8a` / Android API 29, and packages the app as `org.timecrisis2.frame`. It validates the pinned SDL 2.30.11 and OpenXR 1.1.43 archives by SHA-256 and records source, upstream, toolchain, dependency, and APK identity in the build receipt. The Frame signing key lives at `~/.config/timecrisis2vr/steam-frame-debug.keystore`.

The APK and installer bundle are ROM-free. The launcher accepts a user-selected `timecrs2.zip` or an archive copied to persistent `/sdcard/Documents/TimeCrisis2VR/timecrs2.zip`; `FrameStorage` validates the complete 22-chip hash manifest before importing it. The native process uses that same directory for extracted chips, models, settings, logs, and diagnostic files. A failed import preserves the previous valid archive and extracted set.

## OpenXR and rendering

The Frame profile is conditional on `TCVR_STEAM_FRAME`; normal Quest and PC builds keep their existing paths. Steam Frame uses `/interaction_profiles/valve/frame_controller_valve` and retains `/interaction_profiles/oculus/touch_controller` bindings as fallback. Each eye follows the runtime's recommended dimensions and aspect ratio, caps the largest dimension at 2160 pixels, and respects OpenXR's reported maximum image size. The Frame build does not request a fixed refresh rate: SteamVR chooses the display mode and rendering follows predicted display time.

Frame controller actions are: either trigger selects that hand and fires; either grip leaves cover; right A inserts credit; right B toggles the laser; right Menu opens or resumes the options panel; and right X changes cover mode in options. Left View taps back/resume, a one-second hold requests recentering, and a three-second hold also calibrates upright height. Recenter and calibration provide the standard 0.6-amplitude, 80 ms haptic pulse to both hands. Touch fallback bindings and the non-Frame control paths remain available.

The optional, one-shot `frame-probe.cfg` diagnostic forces a neutral view and neutral input when the app is unfocused, then advances the normal arcade update path. Its defaults are 3600 XR frame steps, 30 seconds, and paired-eye capture after 600 actual arcade updates. Capture requests outside the conservative time/frame budget are disabled; timeout without paired capture is logged. The probe is source/runtime instrumentation only: an unattended capture does not demonstrate tracked controllers or worn play.

## Original cabinet link

The port now supplies a C139/C422 adapter for the original MIPS link RAM at `0x06200000..0x06203fff` and registers at `0x06400000..0x0640000f`. The H8 TSS firmware explicitly disables SCI1; the separate `0x0e800000` sub-board serial status is not this link. Each cabinet runs its own unmodified game ROM, MIPS core, H8 processors, input and render view. The adapter transports the original big-endian packet bytes over TCP; it does not synthesize partner state or share one game camera.

`link/tc2_link.c` implements the DMA setup/completion, receive ring, status and interrupt boundary. The ROM arms START before writing OFFSET/SIZE and commits the transfer when START bit 0 falls; completion is scheduled one emulated millisecond later. Receive data enters the high 8 KiB ring, independently of staged transmit registers. The ROM performs its own packet checksum and game-state validation. The public [`MAME-TC2-Public` C139 reference](https://github.com/pocketjazzy/MAME-TC2-Public/commit/5ff99b9da5b528de1c5be484115ea3dc014efd3a) informed the device research; ROM disassembly and two real core processes provide this checkout's independent evidence. See `NOTICE.md` for attribution.

`tools/tc2_overlay.py` generates only changed upstream C files into the build directory, verifies the exact pinned upstream commit and unique replacement anchors, and leaves the submodule clean. Shared Quest/Frame/PC CMake sources select that overlay and the adapter. Frame source fingerprints include the adapter and generator; both Android manifests declare Internet access and Windows links Winsock. No ROM, generated ROM source, packet trace or private save is checked in.

### Developer activation and limits

Link is off by default (`TC2_LINK` absent or `0`), preserving the solo DIP and register path. Developer-only activation is `TC2_LINK=listen:PORT` on red and `TC2_LINK=connect:IPV4:PORT` on blue, with `TC2_LINK_SIDE=0` and `1` respectively. Start the listener first. Activation sets the original active-low link DIP (`0x08`) and cabinet-side JVS bit (`0x4000`). Normal transport uses nonblocking sockets and bounded queues; it never waits for a partner frame. Disconnect clears pending transport bytes and leaves the original ROM to handle link loss.

This is not yet a player-facing LAN session: there is no discovery, compatibility handshake, reconnect/reset API, lobby or in-app activation. Invalid configuration/bind currently terminates developer activation; lobby integration must replace that with visible errors, clear session/socket/ring state on reset, and check compatibility before forwarding records. Controller-only lobby variations and component/IP-entry states exist as synthetic, ignored previews under `artifacts/ui-preview`; implementation awaits the user's mockup and asset approval. Steam Frame manual IP entry must work without an OS keyboard.

### Reproducible real-core gate

After preparing the user's own ROM and generated sources, build the software-rendered headless target with:

```sh
cmake -S headless -B build/headless -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/headless --parallel 3
python3 tests/test_tc2_link.py
python3 tests/test_tc2_overlay.py
python3 tools/run_tc2_link.py --frames 4000 --paced --output build/link-pair-proof \
  --press '1@1200+5,1@1230+5,1@1260+5,1@1290+5,10@1400+5,10@1800+5,10@2100+5,20@1500+2000' \
  --blue-press '1@1200+5,1@1230+5,1@1260+5,1@1290+5,10@1400+5,10@1800+5,10@2100+5,20@1500+2500'
python3 tests/check_tc2_link_evidence.py build/link-pair-proof
```

The runner gives both processes separate fresh NVRAM and shared read-only ROM inputs. `--paced` uses independent 59.906 Hz clocks, 150 ms blue startup skew and deterministic 0–3 ms jitter; it disables the diagnostic frame barrier. Omit `--paced` for a test-only barrier run (`TC2_LINK_SYNC=1`); that mode is not a production timing strategy. Headless observation counts the real ROM checksum-success dispatcher at `0x8000b8f0` and validator entry at `0x8000bf38`; it never modifies instructions or game RAM.

The verified binary SHA-256 is `3ef271be02c4f30efdb295180d3774268ad2dfa8732d0cd18b1ffe434cfa0045` (source based on `15d7e1d`, with this milestone's changes). Private evidence remains in ignored build directories:

| Run | Result |
| --- | --- |
| `build/link-pair-paced-final` | Both cores completed 4,000 frames / 11,307,681,000 instructions with zero traps/FP exceptions in 67.578 s. Red/blue accepted 3,002/3,030 packets through the ROM success dispatcher; original game mode was 2, freshness 4, gameplay drift near 1–2. |
| `build/link-barrier-final` | Both cores completed 4,000 frames with zero traps/FP exceptions; 3,008/2,977 accepted packets. The same packet/state evidence checker passed. |
| `build/link-solo-final` | 1,500 frames, zero traps/FP exceptions, no link traffic. All four software captures match the earlier solo baseline byte for byte. |

The checker verifies accepted packets, fresh original two-cabinet game state, exact peer transmit/receive byte order, and distinct final capture hashes. Visual inspection of the paced pair's `red/f03999.png` and `blue/f03999.png` showed red in cover with the pedal prompt while blue remained exposed and firing, with distinct original cabinet viewpoints. Thus this is real linked gameplay and independent local input in a host test, beyond socket activity. It does not establish headset rendering, real LAN latency/loss, cross-platform interoperability, worn controls or campaign completion.

## Evidence boundary

The production input, ROM import/storage, bundle validation, and eye-dimension paths have host fixtures under `tests/test_frame_*.py`; the Android native library has a separate CMake build under `build/steam-frame/native`. These checks establish source and build behavior only. Update this document when APK verification, unattended launch, controller operation, or worn acceptance is completed; do not infer those results from a successful compile or synthetic capture.

The ROM-free ARM64/v2/16 KiB Frame APK from `15d7e1d` (SHA-256 `4aff37b9c082b7e96cd65a680c006aa541de946a5c6770e2121ef5c39b493755`) was installed using the verified bundle. Runtime/controller/worn acceptance is still pending; it predates the cabinet adapter. An initial helper launch URI mistakenly contained a literal `\n`, causing Steam's ShowGameArgs prompt; those probes were cleaned up and baseline restored. The correct shortcut URI is `steam://rungameid/13919115036359393280`; the malformed prompt is not a required normal launch confirmation.
