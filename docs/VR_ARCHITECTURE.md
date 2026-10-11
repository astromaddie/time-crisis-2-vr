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

### Player-facing LAN session

The approved stacked lobby is shared by Frame, Quest and Windows. Open options, move the right stick right to LAN Play, then choose Host, Find or Join by IP. The right stick moves focus; right A or either trigger selects, right B goes back, and Frame View also goes back. Desktop arrows/Enter/Escape use the same model. Back from the lobby returns to options; Start or Return to Game resumes play. During a linked game, options neutralize local input while the arcade clock continues, so a menu does not stall the other cabinet.

Host listens on TCP 32123; Find uses bounded UDP discovery on the same port. Discovery selects the responding IPv4 source, and manual join uses a validated dotted IPv4 field. Both players must ready before the red host can start. The normal path checks a versioned protocol/core/ROM/port identity (`TC2VR-LAN2/core0837a397/timecrs2-tss3verb/port1`) before forwarding any original C422 records; the game's 22-chip ROM manifest was already validated before opening the host. Behavior-changing link revisions must change this identity. Invalid addresses, busy ports, incompatible peers, negotiation/start timeouts and disconnects remain in the lobby as recoverable errors. No normal network operation blocks the rendering thread.

Start returns from a completed host-frame boundary to the original power-on entry. The renderer worker is drained before the lifted C stack unwinds; explicit overlays restore CPU/shadow-stack/interrupt/timer, H8/peripheral, chip, memory and render-list state, preserving loaded ROM resources and the host's settings. Both original cabinets then boot normally; the existing credit/trigger controls start arcade play. Leave resets to solo; reconnect creates a fresh transport/session and resets both cabinets at Start. Default save files are isolated as `tc2_backup.nv`, `tc2_link_red.nv` and `tc2_link_blue.nv`. The developer `TC2_BACKUP` override still takes precedence.

The custom digit editor was removed at the user's request. `quest_keyboard.c` accepts ordinary SDL text, backspace, Enter/cancel and desktop Ctrl/Cmd+V paste. Quest retains its SDL Android IME request. Frame now lazily loads `/data/steamvr/runtime/bin/androidarm64/libopenvr_api.so`, the Lepton mount of the installed SteamVR Android ARM64 API. Windows optionally loads `openvr_api.dll` beside the executable using an absolute path and restricted DLL search; physical input remains available when absent. No runtime binary is bundled. The unmodified official OpenVR v2.5.1 C header and license are pinned at `ae46a8dd0172580648c8922658a100439115d3eb` under `quest/vendor/openvr/`, included by the native source fingerprint. The bridge requests exactly `FnTable:IVROverlay_027`, matching the installed example header; it does not cast newer interface tables to that ABI.

The keyboard owns a hidden event overlay and calls modal `ShowKeyboardForOverlay`; it polls only that handle. Done reads `GetKeyboardText` and replaces the IP field, then requires explicit A/Enter to connect. Closed without Done preserves the prior field and cancels back to the lobby. Invalid addresses can reopen the native editor. Controller actions and SDL edits are suppressed during the modal keyboard; fresh controller release is required afterward. Existing OpenVR clients are borrowed; initialization as an Overlay occurs only after `Init_NotInitialized`, and shutdown occurs only for a client the bridge initialized whose token remains current. Token changes invalidate pointers without calling stale interfaces. It neither accesses the OpenVR compositor nor changes the OpenXR scene lifecycle. Load/API/init/show errors appear in the lobby and include detailed `[vr-keyboard]` diagnostics.

**Device acceptance remains pending.** Installed library/mount/export evidence establishes the intended route, but Android linker namespace access, `IVROverlay_027` availability in the installed runtime, keyboard placement/display, controller typing and coexistence with the OpenXR scene have not been tested in the headset. Windows popup and Quest IME display also remain runtime-unverified. Discovery and selecting a discovered host do not require a keyboard.

The original raw developer path remains available with `TC2_LINK=listen:PORT` / `connect:IPV4:PORT` and `TC2_LINK_SIDE=0|1`. It is a diagnostic protocol without the lobby handshake and must only pair with the same raw mode. Link remains off by default. No ROM, packet fabrication or game-state patch is used by either mode.

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

### Session and UI acceptance evidence

`tests/test_tc2_session.py` exercises real loopback TCP/UDP with ASan/UBSan: discovery, compatibility, readiness/start, two reconnect cycles, premature game-record rejection, version mismatch, negotiation/start timeout and silent-peer loss. `tests/test_quest_lobby.py` exercises the production rasterizer and normal text events: paste, address limits/normalization, invalid input, backspace, Enter/cancel, readiness and errors. Its synthetic UI captures are in `build/lobby-render/`; the actual rendered home, ready and ordinary IP field were inspected. `tests/test_openvr_keyboard.py` exercises the production dynamic loader and pinned function table against a fake shared runtime with ASan/UBSan: owned/borrowed clients, failure cleanup, Done/cancel/reopen, token invalidation, and production lobby modal input/replacement/explicit-connect behavior. `build/openvr-keyboard/keyboard-done.png` is the inspected production field after synthetic Done, not a rendered SteamVR keyboard.

The real-core session gate is `python3 tests/run_tc2_session_gate.py`, followed by `python3 tests/check_tc2_session_evidence.py build/session-core-proof`. It ran two independently paced cores through initial solo → negotiated linked gameplay → disconnect/reconnect → a second negotiated game → solo. Each game ran about 4,000 frames; final solo ran 1,500. The checker independently requires each of the four game segments to have ROM-accepted packets and original mode 2/freshness 4/drift below the ROM's 17-frame threshold, then verifies clean solo exit with no link traffic, restored solo backup path and separate cabinet save files. Both cores passed with zero traps/FP exceptions. The report records the tested binary hash; logs, captures and private saves remain ignored under `build/session-core-proof/`.

A separate fresh-boot comparison in `build/reset-proof/` ran two cold resets in one process and matched fresh captures byte for byte at frames 600, 1,000 and 1,499. The shared input fixture passes in both Frame and Quest/Touch configurations (`tests/test_frame_profile.py`, also with `--touch`). Android host/UI/keyboard/window objects and Windows adapter/lobby/keyboard objects compile. The final Frame native library linked successfully (SHA-256 `41e443ae64c471a64f90195301a7025eb430740ccde1f33793007f115672672e`; `build/lobby-frame-build.log`); that hash predates the direct OpenVR keyboard bridge. The bridge and changed host/keyboard objects compile for Frame, and the bridge/keyboard objects compile for Windows; final packaging and device checks are separate. These are host checks, not real LAN, SteamVR keyboard, headset rendering or worn input acceptance.

## Evidence boundary

The production input, ROM import/storage, bundle validation, and eye-dimension paths have host fixtures under `tests/test_frame_*.py`; the Android native library has a separate CMake build under `build/steam-frame/native`. These checks establish source and build behavior only. Update this document when APK verification, unattended launch, controller operation, or worn acceptance is completed; do not infer those results from a successful compile or synthetic capture.

The ROM-free ARM64/v2/16 KiB Frame APK from `15d7e1d` (SHA-256 `4aff37b9c082b7e96cd65a680c006aa541de946a5c6770e2121ef5c39b493755`) was installed using the verified bundle. Runtime/controller/worn acceptance is still pending; it predates the cabinet adapter. An initial helper launch URI mistakenly contained a literal `\n`, causing Steam's ShowGameArgs prompt; those probes were cleaned up and baseline restored. The correct shortcut URI is `steam://rungameid/13919115036359393280`; the malformed prompt is not a required normal launch confirmation.


### Installed native-keyboard build (2026-10-11)

The clean `176212d` Frame APK was built, v2-signature/16 KiB alignment/AArch64 dependency closure checked, and installed with verified title/data backups. APK SHA-256: `2881ba87e258de9b6decd2d40db2e4c636aadbdd6e53aa2da1d96a54bd0828ae`. The ROM-free bundle and 22-chip user-ROM dry run passed; OpenVR license staging was checked. This replaces the earlier installed solo APK, which remains preserved locally under `artifacts/steam-frame-preserved-15d7e1d`.

The authorized off-head test sent the correct Steam URI once at 23:41:26 JST. Steam recorded it, but no LaunchApp action, Lepton process/container, title log or capture appeared within the bounded startup observation. There was no new ShowGameArgs/ShowInterstitials message, and the one-shot probe was never consumed. Its unused config was removed; persistent data and title contents/ownership/modes were verified identical to the pre-probe snapshot. The launch cause is undetermined. This is installation evidence only: native keyboard popup, game rendering, physical input and cross-device LAN are still unverified. The user was asked to launch the title from the headset library.

Curated local evidence: `artifacts/steam-frame/evidence/launch-20261011-2341/steam-console.txt` and `device-runtime.txt`. Pre-install backup on Frame: `/home/steamos/Documents/TimeCrisis2VR-backups/20261011T143959Z`; pre-probe snapshot: `/home/steamos/Documents/TimeCrisis2VR-backups/20261011T1442Z-preprobe`. Device access uses the user's explicitly authorized SteamOS Devkit identity, without changing authentication configuration.
