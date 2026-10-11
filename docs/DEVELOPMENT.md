# Development and validation

## Active code

- `upstream/`: pinned [namco22-decompile](https://github.com/spacestate1/namco22-decompile) submodule at `0837a397ebe554dbf5341d95629aa1e29fc58a52`.
- `quest/tc2_sources.cmake`: authoritative System 23 source list.
- `quest/tc2_scene.c` and `quest/tc2_gl.c`: arcade geometry reconstruction and VR rendering.
- `quest/quest_host.c`: shared OpenXR host, frame pacing and controls.
- `quest/quest_gun.c`, `quest/tc2_weapon.h`: animated weapons and arcade weapon-state decoding.
- `pc/`: Windows entry point, OpenGL loader and CMake build.
- `tools/prepare_tc2.py`: validates the 22 ROM chips and prepares the native game sources.

TC1-specific files such as `tools/patch_upstream.py`, `tools/translate_crate.py`, `quest/quest_scene.c` and the historical TC1 test scripts are inherited reference material. They are not part of the active TCII build unless listed in `tc2_sources.cmake`.

## Windows build environment

Install Git, Python 3 and CMake on PATH. Clone recursively, or run `git submodule update --init upstream` after cloning. The Windows build fetches its pinned SDL2, OpenXR loader, Ninja, LLVM-MinGW and zlib dependencies into `.tools/`.

For Android, install a JDK and set `JAVA_HOME`. Install Android SDK **platform 35**, **build-tools 35.0.1** and **NDK 28.2.13676358**. Place the SDK at `.tools/android-sdk`, or create a junction to an existing SDK:

```powershell
New-Item -ItemType Directory -Force .tools
New-Item -ItemType Junction -Path .tools/android-sdk -Target "$env:LOCALAPPDATA\Android\Sdk"
```

Use the SDK's `sdkmanager` or Android Studio SDK Manager to install the required versions. The build scripts do not install these SDK components automatically. Do not delete an existing SDK when configuring this path.

Build using the arcade `timecrs2.zip`:

```powershell
python -X utf8 tools/build_pc.py --rom "$env:USERPROFILE\Downloads\timecrs2.zip" --jobs 2
python -X utf8 tools/build.py --rom "$env:USERPROFILE\Downloads\timecrs2.zip" --bundle-roms --jobs 2
```

After successful preparation, `--skip-prepare` reuses the validated generated inputs. `tools/build.py --package-only --bundle-roms` repackages the already-built Android libraries. Omitting `--bundle-roms` produces a ROM-free APK with a ZIP import screen; public v0.2.0 downloads use the bundled variant.

Outputs:

- `artifacts/bundled/TimeCrisis2VR-with-ROM.apk`
- `artifacts/pc/TimeCrisis2VR-PC/TimeCrisis2VR.exe`
- `artifacts/pc/TimeCrisis2VR-v0.2.0-windows-x64.zip`

The ROM preparer checks chip sizes and CRCs against the upstream table, records SHA-256 hashes in `build/rom-audit.json` and accepts equivalent filenames by matching content. ROMs, generated sources, keys and build output remain outside Git. APK signing uses the local `.tools/debug.keystore`; retain the same private key to update existing installations.

## Steam Frame ARM64 Android flavor

`tools/build_frame.py` is a separate macOS-hosted Frame build. It reuses the pinned TC2 source and a caller-selected shared `.tools` dependency cache without copying it, validates the SDL 2.30.11 and OpenXR 1.1.43 archive hashes, and targets `arm64-v8a` / Android API 29. The expected local SDK is platform 34, build-tools 35.0.0, NDK 27.2.12479018, and JDK 17. Pass the ROM explicitly with `--rom`; generated native inputs are kept under `build/steam-frame`.

```sh
python3 tools/build_frame.py --rom "$HOME/Downloads/timecrs2.zip" --shared-tools /path/to/time-crisis-vr/.tools --jobs 2
python3 tools/steam_frame/bundle/install.py --verify-only
python3 tools/steam_frame/bundle/install.py --dry-run --rom "$HOME/Downloads/timecrs2.zip"
```

The output APK at `artifacts/steam-frame/TimeCrisis2VR-SteamFrame-arm64.apk` includes the 22-chip hash manifest and weapon models, but no ROM chips or archive. The launcher imports a selected archive into persistent `Documents/TimeCrisis2VR`; the native process uses that same directory for extracted chips, configuration, logs and diagnostic captures. `--package-only` requires a source fingerprint receipt written by the native build and rejects stale libraries. The Frame signing key is retained at `~/.config/timecrisis2vr/steam-frame-debug.keystore`, outside disposable build caches.

For an unattended diagnostic pass, put `force_render=1`, `capture_once=1`, `capture_after_ticks=600`, `exit_after_frames=3600`, and `exit_after_seconds=30` in `frame-probe.cfg` in the persistent game directory before launch. The Frame-only probe advances the arcade with neutral input even when normal focus or tracking is unavailable, then captures both eyes after 600 actual game updates. It exits at the first frame or time limit and logs separately when it times out without a paired capture. Requested capture ticks that cannot fit the configured bounds are disabled. This is an unattended rendering diagnostic, not proof of normal controller tracking or worn play.

## Validation

```powershell
python -X utf8 tests/test_handedness.py
python -X utf8 tests/verify_apk.py
```

The input fixture compiles the real host input code with a mock OpenXR runtime. It checks both hands, trigger handoffs, grip cover, pitch, saved layouts, tracking/focus loss and sparse replay preservation. The APK verifier checks all chip hashes, both weapon assets, ARM64 libraries, the immersive entry point, DEX and the package hash. Packaging additionally verifies the signature and 16 KiB ZIP alignment.

`tests/test_frame_profile.py` also compiles the production host input path with `TCVR_STEAM_FRAME` against a mock runtime, checking both profile bindings, Touch fallback, Frame control semantics, View hold timing and haptics. `tests/test_frame_storage.py` compiles and runs the production Java ROM importer/storage adapter against synthetic 22-chip archives; `tests/test_frame_dimensions.py` checks the 2160-per-eye target cap and runtime limits. `tests/test_frame_bundle.py` exercises the ROM-free installer verifier and dry-run without touching a device.

`tests/tc2_scene_fixture.c` covers projection, aiming, HUD and mask classification. Compile it with `quest/tc2_scene.c` and the Quest / upstream TCII include paths. `tests/tc2_weapon_fixture.c` covers the arcade weapon-state decoder. The production GLES fixtures use MSVC, Pillow and ANGLE DLLs from VS Code; see [transition validation](TRANSITIONS.md) and [weapon validation](WEAPONS.md). Automated checks complement headset testing.

v0.2.0 preserves the native game code tested in `0.1.0-test.4`. Quest 3 gameplay and a Windows PCVR session through Virtual Desktop were tested. A complete-game playthrough remains pending.

## Diagnostics and replay

```powershell
./Export-Quest-Diagnostics.ps1
adb shell run-as org.timecrisis2.arcadevr cat files/timecrisis2-vr.log
adb shell run-as org.timecrisis2.arcadevr touch files/capture.request
```

The exporter accepts `-Adb`, `-Serial` and `-Output`. It saves current/previous logs and input recordings, options and Android exit information locally. The capture request writes `eye-0.ppm` / `eye-1.ppm` in app-private files and can briefly stall rendering.

On Windows, `tc2.log`, `tc2-previous.log`, `tc2_last.rec`, `tc2_prev.rec` and cabinet backup RAM reside beside the executable. To replay:

```powershell
./TimeCrisis2VR.exe --desktop --replay session.rec --frames 12000
```

Recordings store arcade input changes, not the starting cabinet backup RAM, headset poses or video. Exact reproduction needs matching cabinet settings. Live input is suppressed during replay so held recorded inputs survive frames without a new entry.

For local graphics analysis, `TCVR_SCENE_VIEW=1` enables reconstructed geometry in desktop mode and `TCVR_FAST=1` disables real-time pacing. `TC2_FRAME_CAPTURE` and `TC2_FRAME_RANGE` save renderer snapshots; details are in [TRANSITIONS.md](TRANSITIONS.md). Keep these ROM-derived diagnostic files in ignored local folders.
