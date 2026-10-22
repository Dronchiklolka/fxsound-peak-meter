// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Dronchiklolka
#define NOMINMAX
#include <Windows.h>
#include "../dsp/include/DfxDsp.h"
// The legacy status-code header defines an unused static diagnostic helper.
#pragma warning(push)
#pragma warning(disable: 4505)
#include "../dsp/ptutil/include/codedefs.h"
#pragma warning(pop)
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    // Redirect legacy HKCU access in this process only, preserving real settings.
    class RegistrySandbox
    {
    public:
        RegistrySandbox()
            : path_(L"Software\\FxSoundPeakMeterTest-" + std::to_wstring(GetCurrentProcessId()))
        {
            require(RegCreateKeyExW(HKEY_CURRENT_USER, path_.c_str(), 0, nullptr,
                REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &key_, nullptr) == ERROR_SUCCESS,
                "create isolated test registry");
            if (RegOverridePredefKey(HKEY_CURRENT_USER, key_) != ERROR_SUCCESS)
            {
                RegCloseKey(key_);
                RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str());
                throw std::runtime_error("redirect test process registry");
            }
        }
        ~RegistrySandbox()
        {
            RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
            RegCloseKey(key_);
            RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str());
        }
        RegistrySandbox(const RegistrySandbox&) = delete;
        RegistrySandbox& operator=(const RegistrySandbox&) = delete;
    private:
        std::wstring path_;
        HKEY key_ = nullptr;
    };

    void process(DfxDsp& dsp, std::vector<float>& input, std::vector<float>& output, int frames)
    {
        require(dsp.processAudio(reinterpret_cast<short*>(input.data()),
            reinterpret_cast<short*>(output.data()), frames, 0) == OKAY, "DSP processing succeeds");
    }
}

int main()
{
    try
    {
        RegistrySandbox sandbox;
        DfxDsp dsp;
        require(dsp.setSignalFormat(32, 2, 48000, 32) == OKAY, "float stereo format");
        dsp.powerOn(false);
        dsp.eqOn(true);
        std::vector<float> input(128, 0), output(128, 0);
        process(dsp, input, output, 64); // Finish initial legacy settings refresh.
        dsp.setMasterGain(-6.0f);
        dsp.resetClipping();
        input.assign(128, 0.25f);
        input.front() = 1.25f;
        input.back() = -1.5f;
        const auto original = input;
        process(dsp, input, output, 64);
        auto meter = dsp.getAudioMeter();
        require(input == original, "separate input buffer is unchanged");
        require(meter.stages[0].overSamples == 2 && meter.stages[0].maximum == 1.5f,
                "real input tap catches overs before processing");
        const auto& afterEq = meter.stages[1];
        require(afterEq.observed && afterEq.maximum > 0.70f && afterEq.maximum < 0.80f,
                "real post-EQ tap observes master gain attenuation");
        require(afterEq.overSamples == 0 && meter.stages[2].overSamples == 0,
                "attenuated input overs are not false output overs");
        require(meter.stages[2].observed, "real output tap is connected");

        require(dsp.setSignalFormat(32, 2, 48000, 32) == OKAY, "repeat format");
        input.assign(128, 0);
        process(dsp, input, output, 64);
        require(dsp.getAudioMeter().stages[0].overSamples == 2, "history survives per-buffer format calls");
        dsp.resetClipping();
        require(dsp.getAudioMeter().stages[0].maximum == 0, "reset visible without another block");
        dsp.setMasterGain(12.0f);
        input.assign(128, 0.5f);
        process(dsp, input, input, 64);
        meter = dsp.getAudioMeter();
        require(meter.stages[0].maximum == 0.5f && meter.stages[0].overSamples == 0,
                "in-place DSP keeps the original input reading");
        require(meter.stages[1].overSamples == 128 && meter.stages[1].maximum > 1.9f,
                "real EQ gain creates independently reported overs");

        require(dsp.setSignalFormat(32, 2, 1000, 32) == OKAY, "unsupported-rate passthrough format");
        input.assign(128, 0.2f);
        output.resize(input.size());
        process(dsp, input, output, 64);
        meter = dsp.getAudioMeter();
        require(meter.stages[0].observed && meter.stages[2].observed && !meter.stages[1].observed,
                "unsupported DSP format is marked as an absent EQ tap");

        require(dsp.setSignalFormat(32, 8, 96000, 32) == OKAY, "8-channel format");
        input.assign(512, 0);
        input.back() = -1.1f;
        output.resize(input.size());
        process(dsp, input, output, 64);
        meter = dsp.getAudioMeter();
        require(meter.channels == 8 && meter.stages[0].overSamples == 1,
                "rear-channel over reaches the real input tap");
        require(meter.stages[1].observed, "multichannel EQ tap is connected");
        std::cout << "PASS: offline DSP taps, master gain, in-place audio, format history, "
                     "reset, unsupported-format passthrough and 8-channel input; no audio device opened\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
