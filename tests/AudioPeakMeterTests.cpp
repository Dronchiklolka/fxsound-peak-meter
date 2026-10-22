// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Dronchiklolka
#include "../dsp/include/AudioPeakMeter.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    bool near(float a, float b, float tolerance = 0.00001f)
    {
        return std::fabs(a - b) < tolerance;
    }

    void process(DfxAudioMeter& meter, const float* samples, int frames, int channels)
    {
        require(meter.beginBlock(frames), "valid block accepted");
        for (int stage = 0; stage < DfxAudioMeter::StageCount; ++stage)
            meter.observe(static_cast<DfxAudioMeter::Stage>(stage), samples,
                          static_cast<std::size_t>(frames) * channels);
        meter.endBlock();
    }
}

int main()
{
    std::atomic<float> floatAtomic{0};
    require(floatAtomic.is_lock_free(), "float publication is lock-free on this build");
    DfxAudioMeter meter;
    const float silence[] = {0, 0, 0, 0};
    require(!meter.beginBlock(2), "unconfigured meter disabled");
    meter.endBlock();
    require(meter.snapshot().blocks == 0, "invalid block cannot publish");
    meter.configure(16, 2, 48000);
    require(!meter.beginBlock(2), "integer PCM is not read as float");
    meter.configure(32, 9, 48000);
    require(!meter.beginBlock(2), "unsupported channel count disabled");
    meter.configure(32, 2, 0);
    require(!meter.beginBlock(2), "invalid sample rate disabled");
    meter.configure(32, 2, 48000);
    require(!meter.beginBlock(0), "empty block rejected");
    process(meter, silence, 2, 2);
    require(near(meter.snapshot().stages[0].peak, 0), "silence has no peak");
    require(meter.snapshot().stages[0].observed, "measured silence is distinct from an absent tap");
    require(near(DfxAudioMeter::decibels(0), -120), "finite silence floor for JSON");

    const float boundaries[] = {1.0f, -1.0f, 0.5f, -0.25f};
    process(meter, boundaries, 2, 2);
    auto snapshot = meter.snapshot();
    require(snapshot.stages[0].fullScaleSamples == 2, "exact full scale counted");
    require(snapshot.stages[0].overSamples == 0, "exact full scale is not an over");
    require(near(snapshot.stages[0].maximum, 1), "held full-scale peak");
    require(near(DfxAudioMeter::decibels(0.5f), -6.0206f, 0.0001f), "dBFS conversion");

    const float excess[] = {1.25f, -1.5f, 0.2f, 0};
    process(meter, excess, 2, 2);
    snapshot = meter.snapshot();
    require(snapshot.stages[0].overSamples == 2, "positive and negative overs counted");
    require(snapshot.stages[0].fullScaleSamples == 2, "overs do not inflate the exact-full-scale counter");
    require(near(snapshot.stages[0].maximum, 1.5f), "over-range peak retained");
    const auto history = snapshot.stages[0].overSamples;
    meter.configure(32, 2, 48000);
    process(meter, silence, 2, 2);
    snapshot = meter.snapshot();
    require(snapshot.stages[0].overSamples == history, "per-block format call preserves history");
    require(near(snapshot.stages[0].maximum, 1.5f), "format call preserves held maximum");
    require(snapshot.stages[0].peak < 1.5f && snapshot.stages[0].peak > 0,
            "current peak decays while held maximum persists");

    const float invalid[] = {std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 0.5f};
    process(meter, invalid, 2, 2);
    snapshot = meter.snapshot();
    require(snapshot.stages[0].invalidSamples == 3, "non-finite samples flagged separately");
    require(std::isfinite(snapshot.stages[0].peak), "invalid audio cannot poison meter values");
    require(std::isnan(invalid[0]) && std::isinf(invalid[1]), "observer never modifies audio");

    meter.requestReset();
    require(meter.snapshot().stages[0].overSamples == 0, "reset visible with audio stopped");
    require(meter.snapshot().stages[0].maximum == 0, "stopped reset clears held maximum");
    process(meter, silence, 2, 2);
    require(meter.snapshot().stages[0].invalidSamples == 0, "reset applied on next block");

    meter.configure(32, 1, 44100);
    float inPlace[] = {0.5f, -0.25f};
    require(meter.beginBlock(2), "mono block accepted");
    meter.observe(DfxAudioMeter::Input, inPlace, 2);
    inPlace[0] = 1.2f;
    meter.observe(DfxAudioMeter::AfterEq, inPlace, 1);
    meter.observe(DfxAudioMeter::AfterEq, inPlace + 1, 1);
    inPlace[0] = 0.9f; // Model an output limiter following the EQ stage.
    meter.observe(DfxAudioMeter::Output, inPlace, 2);
    meter.endBlock();
    snapshot = meter.snapshot();
    require(near(snapshot.stages[0].maximum, 0.5f), "input captured before in-place processing");
    require(snapshot.stages[1].overSamples == 1, "split EQ chunks accumulate within a block");
    require(snapshot.stages[2].overSamples == 0, "limiter output does not hide earlier EQ over");
    require(near(snapshot.stages[2].maximum, 0.9f), "final output measured independently");

    meter.configure(32, 8, 96000);
    float surround[16] = {};
    surround[15] = -1.1f;
    process(meter, surround, 2, 8);
    snapshot = meter.snapshot();
    require(snapshot.channels == 8 && snapshot.sampleRate == 96000, "new format published");
    require(snapshot.stages[0].overSamples == 1, "rear-channel overs are measured");

    meter.configure(32, 4, 48000);
    require(meter.beginBlock(1), "passthrough quad block accepted");
    meter.observe(DfxAudioMeter::Input, silence, 4);
    meter.observe(DfxAudioMeter::Output, silence, 4);
    meter.endBlock();
    snapshot = meter.snapshot();
    require(snapshot.stages[0].observed && snapshot.stages[2].observed,
            "passthrough input and output remain available");
    require(!snapshot.stages[1].observed, "a skipped EQ tap is not reported as measured silence");

    DfxAudioMeter concurrent;
    concurrent.configure(32, 2, 48000);
    std::atomic<bool> finished{false};
    std::thread producer([&]
    {
        const float samples[] = {0.4f, -1.1f, 0.2f, 0};
        for (int i = 0; i < 20000; ++i)
        {
            concurrent.configure(32, 2, 48000);
            process(concurrent, samples, 2, 2);
        }
        finished.store(true, std::memory_order_release);
    });
    unsigned reads = 0;
    do
    {
        const auto read = concurrent.snapshot();
        for (const auto& stage : read.stages)
            require(std::isfinite(stage.peak) && std::isfinite(stage.maximum),
                    "concurrent snapshots remain finite");
        if ((++reads % 101) == 0)
            concurrent.requestReset();
    } while (!finished.load(std::memory_order_acquire));
    producer.join();
    require(concurrent.snapshot().blocks == 20000, "all audio blocks published under concurrent reads");
    std::cout << "PASS: thresholds, reset, repeated format, in-place/split buffers, "
                 "mono/stereo/8ch, non-finite samples, decay and concurrent readers\n";
    return 0;
}
