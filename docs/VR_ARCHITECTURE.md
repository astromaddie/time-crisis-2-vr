# Steam Frame architecture

## Target and build

The Frame flavor is a separate macOS-hosted ARM64 Android/OpenXR build of the existing Time Crisis II source. `tools/build_frame.py` enables `TCVR_STEAM_FRAME`, targets `arm64-v8a` / Android API 29, and packages the app as `org.timecrisis2.frame`. It validates the pinned SDL 2.30.11 and OpenXR 1.1.43 archives by SHA-256 and records source, upstream, toolchain, dependency, and APK identity in the build receipt. The Frame signing key lives at `~/.config/timecrisis2vr/steam-frame-debug.keystore`.

The APK and installer bundle are ROM-free. The launcher accepts a user-selected `timecrs2.zip` or an archive copied to persistent `/sdcard/Documents/TimeCrisis2VR/timecrs2.zip`; `FrameStorage` validates the complete 22-chip hash manifest before importing it. The native process uses that same directory for extracted chips, models, settings, logs, and diagnostic files. A failed import preserves the previous valid archive and extracted set.

## OpenXR and rendering

The Frame profile is conditional on `TCVR_STEAM_FRAME`; normal Quest and PC builds keep their existing paths. Steam Frame uses `/interaction_profiles/valve/frame_controller_valve` and retains `/interaction_profiles/oculus/touch_controller` bindings as fallback. Each eye follows the runtime's recommended dimensions and aspect ratio, caps the largest dimension at 2160 pixels, and respects OpenXR's reported maximum image size. The Frame build does not request a fixed refresh rate: SteamVR chooses the display mode and rendering follows predicted display time.

Frame controller actions are: either trigger selects that hand and fires; either grip leaves cover; right A inserts credit; right B toggles the laser; right Menu opens or resumes the options panel; and right X changes cover mode in options. Left View taps back/resume, a one-second hold requests recentering, and a three-second hold also calibrates upright height. Recenter and calibration provide the standard 0.6-amplitude, 80 ms haptic pulse to both hands. Touch fallback bindings and the non-Frame control paths remain available.

The optional, one-shot `frame-probe.cfg` diagnostic forces a neutral view and neutral input when the app is unfocused, then advances the normal arcade update path. Its defaults are 3600 XR frame steps, 30 seconds, and paired-eye capture after 600 actual arcade updates. Capture requests outside the conservative time/frame budget are disabled; timeout without paired capture is logged. The probe is source/runtime instrumentation only: an unattended capture does not demonstrate tracked controllers or worn play.

## Cabinet link research

No multiplayer implementation is included. The TC2 H8 TSS firmware explicitly disables SCI1; P5 toggles around the SCI0 local peripheral transfers, so the earlier SCI1/RS-485 theory is not supported. The likely cabinet-link boundary is the main MIPS C139/C422 device: RAM at `0x06200000..0x06203fff` and registers at `0x06400000..0x0640000f`. This checkout currently supplies RAM/register and interrupt behavior, not a peer protocol. Do not treat the separate `0x0e800000` sub-board serial status as a cabinet link.

The public [`MAME-TC2-Public` C139 reference](https://github.com/pocketjazzy/MAME-TC2-Public/commit/5ff99b9da5b528de1c5be484115ea3dc014efd3a) reports `DSW 0x08` as the active-low link enable and `JVS_PLAYER1 0x4000` as the cabinet ID (left/red clear; right/blue set). That fork's linked-game report is useful evidence, not verification in this checkout. The ROM-generated H8 sources do not establish the channel protocol here, and there is no two-cabinet test; multiplayer remains unverified.

## Evidence boundary

The production input, ROM import/storage, bundle validation, and eye-dimension paths have host fixtures under `tests/test_frame_*.py`; the Android native library has a separate CMake build under `build/steam-frame/native`. These checks establish source and build behavior only. Update this document when APK verification, unattended launch, controller operation, or worn acceptance is completed; do not infer those results from a successful compile or synthetic capture.
