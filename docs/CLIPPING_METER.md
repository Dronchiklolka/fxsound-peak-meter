# Sample-peak and overload monitor

The Full view shows input, post-EQ/master-gain (before effects/limiting), and final
DSP output. The observer does not change audio, gain, EQ algorithms or limiting.
English and Russian meter labels are provided; other UI languages use English labels.

## Readings and limits

Current peaks fall at 24 dB/second while audio blocks are processed. The GUI clears
stale current levels after 250 ms without another block; held maxima and counters persist.
A real format change starts a new history; repeated format notifications preserve it.
Reset clears the display immediately even when audio is stopped and applies to audio
state at the next block. `--` marks an unobserved tap, including a reset with no new audio
or a DSP format for which the EQ stage is skipped. It is distinct from measured silence.

OVER counts samples with absolute value >1 and separately recorded NaN/Inf observations.
Exactly +/-1 is counted as full scale and does not alone prove clipping. All input channels
contribute. One sample can contribute at more than one measurement point. An over before
limiting may not reach the output. There is no true-peak reconstruction, analog amplifier
monitoring or reliable diagnosis of distortion already in the recording.

Metering supports the app's 32-bit float buffers with 1..8 channels. It never reinterprets
integer PCM as float. The callback uses preallocated state and atomic publication; new
metering performs no I/O, allocation or locking on that thread. Float atomics are checked
for lock-free operation in the target tests. Published fields are independently atomic,
not a transactional snapshot of the same instant.

## Command line

`FxSound.exe --status` writes `%APPDATA%\FxSound\status.json`. Its `audio_meter` object
includes `kind`, format availability, channels, sample rate, a lifetime block counter,
and `input` / `after_eq` / `output` objects. Each has `observed`, `peak_dbfs`, `max_dbfs`,
`over_samples`, `full_scale_samples`, and `invalid_samples`. dBFS uses a finite -120 floor.
The JSON peak is from the last processed block; check the block counter to detect stale
readings. `FxSound.exe --reset_clipping` resets the running instance. As upstream,
`--status` ignores other options in the same invocation.

## Build and test

Use Visual Studio 2022 C++ v143 Build Tools and Windows SDK, and PowerShell:

```powershell
# Detector only; JUCE and an installed audio driver are not needed.
.\tools\Build-ClipMeter.ps1 -TestOnly

# Build the native libraries and test the real DSP without opening an audio device.
.\tools\Build-ClipMeter.ps1 -DspTestsOnly

# Build the application, run both suites, and prepare a portable folder.
.\tools\Build-ClipMeter.ps1 -JuceModulesPath C:\JUCE\modules -OutputDirectory .\build\portable
```

Use [JUCE 6.1.6](https://github.com/juce-framework/JUCE/tree/6.1.6) modules. A corresponding-source
release archive already includes them under `third_party\JUCE\modules`; pass that path
instead. A full checkout includes upstream default presets under `bin\x64\Factsoft`.
The script does not collect user presets, settings, installed fonts or diagnostic logs.
An already installed official FxSound virtual audio driver is required to **run** the GUI,
not to compile or run the offline tests. Close the target executable before rebuilding it.

## Validation scope

- Detector: silence, exact full scale, overs, non-finite input, reset while stopped,
  unchanged formats, envelope decay, absent taps, in-place/split buffers, 1/2/8 channels,
  and concurrent snapshot/reset readers.
- Real DSP: input overs attenuated by master gain, post-EQ overs from gain, in-place audio,
  history across format calls, stopped reset, unsupported-format passthrough, and 8 channels.
  Test-process HKCU access is redirected to an isolated temporary registry subtree and
  cleaned up on exit; real user settings are not changed. No audio device is opened.
- Windows 11 x64 / JUCE 6.1.6: Release build, meter layout, stopped GUI reset and quiet
  real-device playback. Over-range fixtures are never played through a physical device.
- Windows CI runs detector and offline DSP suites. ARM64/JUCE 8 and other operating systems
  have not been independently validated. No claim of exhaustive defect absence is made.
