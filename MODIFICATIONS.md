# Changes from upstream

Maintainer: [Dronchiklolka](https://github.com/Dronchiklolka).
Modified: **2026-10-22**.
Upstream: [fxsound2/fxsound-app](https://github.com/fxsound2/fxsound-app),
base commit `d8e7a23d37ed5939c2a3090a1c1756c7f2500b17` (FxSound 1.2.15.0).

## Peak Meter 0.1.0

- Observe input, post-EQ/master-gain and final DSP output sample peaks, without
  changing audio buffers, gain or limiting.
- Add held maxima, separate full-scale / over-range / non-finite counters and reset.
- Add Full-view meters and an overload/reset button with English/Russian labels.
- Expose meter readings in `--status` and support `--reset_clipping` on a running app.
- Retain history on repeated format notifications; clear it on real format changes.
- Distinguish an unobserved DSP tap from measured silence.
- Keep idle meter/reset updates alive on the JUCE 8 UI path.
- Add isolated detector tests, offline DSP integration tests and Windows CI.
- Make JUCE's module path configurable, quote post-build paths and remove absolute
  PDB paths from the executable's debug-directory reference.

The driver's implementation, EQ algorithms, effects and preset formats are inherited
from upstream. Personal headphone presets and local machine configuration are not
part of this fork. See [docs/CLIPPING_METER.md](docs/CLIPPING_METER.md) for limits and validation.
