# Peak Meter 0.1.0 audit

Date: 2026-10-22. Scope: changes to FxSound's sample-peak monitoring, UI, CLI,
build/test tooling and public packaging. The inherited driver and unrelated
upstream subsystems were not audited or changed.

## Corrections before publication

1. Preserve history when AudioPassthru repeats the unchanged format each block.
2. Handle `--reset_clipping` on an already-running instance, not only initialization.
3. Distinguish a tap that was not observed from measured silence. A real unsupported
   DSP sample rate is covered by the offline integration test.
4. Poll idle meter/reset state on the JUCE 8 UI path after spectrum updates pause.
   That source path is fixed; an ARM64/JUCE 8 build is not independently verified.
5. Use explicit opaque theme text colors, keeping the meter labels readable.
6. Make test-only modes independent of JUCE/output parameters. Invoke the compiler
   through Developer PowerShell, preserving paths containing spaces and Cyrillic.
   Quote MSBuild post-build copy paths and bound test processes with a timeout.
7. Strip the absolute PDB reference and build public releases in a neutral GitHub
   runner checkout. Refuse to package executables containing user home-directory paths.
8. Count exactly +/-1 separately from >1, so overs do not inflate the full-scale counter.

## Evidence

- Release x64 built locally with VS 2022 v143, Windows SDK and JUCE 6.1.6.
- Detector suite passed under `/W4 /WX /fp:precise`, including concurrent reads/resets.
- Real DSP suite passed: attenuation, EQ-created overs, input/output aliasing,
  repeated formats, stopped reset, unsupported-format passthrough and eight channels.
  HKCU was isolated to the test process and no audio output device was opened.
- Detector-only mode passed from a checkout path containing spaces and Cyrillic.
- GUI layout, current/held readings and stopped reset were checked on Windows 11 x64.
  Quiet physical playback was used; over-range fixtures were kept offline.
- [Public build workflow](https://github.com/Dronchiklolka/fxsound-peak-meter/actions/workflows/peak-meter.yml)
  rebuilds the application, runs both suites and prepares binary plus corresponding-source archives.

## Publication boundary

Packages use an explicit file allowlist: executable, upstream factory presets,
documentation and licenses. No user settings, personal presets, logs, PDBs, backups,
local installers or authentication data are collected. Source archives include the
application sources and exact JUCE modules, without developer build output.

This is a bounded review and test record, not a guarantee that no defects remain.
Sample peaks do not prove the absence of true-peak or analog clipping. Multichannel
DSP wiring is covered offline; physical multichannel devices and ARM64 were not tested.
