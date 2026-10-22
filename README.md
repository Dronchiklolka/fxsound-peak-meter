# FxSound Peak Meter

[На русском](README.ru.md) · [Release notes](https://github.com/Dronchiklolka/fxsound-peak-meter/releases/tag/peak-meter-v0.1.0)

An unofficial [FxSound](https://github.com/fxsound2/fxsound-app) fork with peak
meters before processing, after EQ and at the output. It shows the current level,
held maximum and overload history. The audio processing and driver are unchanged.

**[Download for Windows x64](https://github.com/Dronchiklolka/fxsound-peak-meter/releases/download/peak-meter-v0.1.0/fxsound-peak-meter-0.1.0-windows-x64.zip)** — version 0.1.0, preview.

## Install

1. Install [official FxSound](https://www.fxsound.com/download) if its virtual audio
   driver is not already installed.
2. Exit the running FxSound through its system-tray menu.
3. Extract the **whole ZIP** to a permanent folder and run **FxSound.exe**.
4. Switch to **Full view** to see the meters below the spectrum. If the window is
   hidden, open it from the system tray.

No build tools are needed. Only one FxSound instance can run at a time. If the
original app opens, exit it from the tray and launch the extracted executable again.
The ZIP does not install a driver or replace the official startup shortcut.

Existing FxSound settings and presets on your PC are reused. The download includes
standard factory presets. This community build is unsigned.

## Read the meters

- **Input / After EQ / Output** show the current sample peak and held maximum in dBFS.
- **OVER / Reset** turns red after samples exceed full scale or contain invalid values.
  Click it to clear the history. Samples exactly at full scale are counted separately.
- `-inf` means silence; `--` means no measurement at that point.

The after-EQ meter is before effects and limiting, so an over there may not reach
the output. These are sample-peak measurements: they do not measure inter-sample
peaks, analog distortion or clipping already present in a recording.

[Meter details and command-line options](docs/CLIPPING_METER.md)

## Source and license

Maintained by [Dronchiklolka](https://github.com/Dronchiklolka), based on
[fxsound2/fxsound-app](https://github.com/fxsound2/fxsound-app). This is an independent
fork, not an official FxSound release. The original AGPL license and author notices
are retained: [LICENSE](LICENSE), [NOTICE](NOTICE.md), [modifications](MODIFICATIONS.md), [source history](PROVENANCE.md).

The release includes a matching **source-with-juce.zip** archive with the JUCE 6.1.6
modules and license notices. Build instructions are in the [build guide](docs/CLIPPING_METER.md).
The tested target is Windows 11 x64. CI builds the app and runs detector and offline
DSP tests. ARM64 and JUCE 8 builds have not been validated.

[Report a problem with this fork](https://github.com/Dronchiklolka/fxsound-peak-meter/issues)
· [Original FxSound documentation](https://github.com/fxsound2/fxsound-app/tree/d8e7a23d37ed5939c2a3090a1c1756c7f2500b17)
