// Modified by Dronchiklolka, 2026-10-22: sample-peak monitoring. See MODIFICATIONS.md.
/*
FxSound
Copyright (C) 2025  FxSound LLC

Contributors:
    www.theremino.com (2025)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FxVisualizer.h"
#include "FxController.h"
#include "FxTheme.h"

namespace
{
    bool meterInRussian()
    {
        return FxController::getInstance().getLanguage().startsWithIgnoreCase("ru");
    }

    String meterLevel(float magnitude)
    {
        if (magnitude <= 0.000001f)
            return "-inf";
        const auto db = DfxAudioMeter::decibels(magnitude);
        return (db > 0 ? "+" : "") + String(db, 1);
    }
}

FxVisualizer::FxVisualizer()
{
    band_values_.resize(FxController::NUM_SPECTRUM_BANDS);
    band_graph_.resize(FxController::NUM_SPECTRUM_BANDS * NUM_BARS);

    addAndMakeVisible(clipping_reset_);
    clipping_reset_.setName("Reset clipping counters");
    clipping_reset_.setTooltip("Sample peaks, not true peak. OVER counts samples above 0 dBFS "
        "and invalid samples at the three measurement points. Click to reset.");
    clipping_reset_.onClick = [this]
    {
        FxController::getInstance().resetClipping();
        updateMeters();
        repaint();
    };
    updateMeters();

#if JUCE_MAJOR_VERSION >= 8
    startTimerHz(10); // Refresh reset/stale levels even while the spectrum is paused.
    start();
#else
    calcGradient();
    reset();
    setFramesPerSecond(10);
#endif

    setOpaque(false);
    setSize(WIDTH, HEIGHT);
}

void FxVisualizer::start()
{    
    calcGradient();

#if JUCE_MAJOR_VERSION >= 8
    if (vblank_listener_ == nullptr)
    {
        vblank_listener_ = std::make_unique<juce::VBlankAttachment>(
            this,
            [this](double timestamp)
            {
                if (!isShowing())
                    return;

                static double last_frame_time = 0.0;
                constexpr double fps_interval = 1.0 / 30.0;

                if (timestamp - last_frame_time >= fps_interval)
                {
                    last_frame_time = timestamp;
                    if (FxController::getInstance().isAudioProcessing())
                    {
                        update();
                        repaint();
                    }
                    else
                    {
                        updateMeters();
                        reset();
                        repaint();
                        vblank_listener_.reset();
                    }
                }
            });
    }
#else
    setFramesPerSecond(30);
#endif
}

void FxVisualizer::pause()
{
    calcGradient();

#if JUCE_MAJOR_VERSION >= 8
    reset();
    repaint();

    if (vblank_listener_ != nullptr)
    {
        vblank_listener_.reset();
    }
#else
    setFramesPerSecond(10);
#endif
}

void FxVisualizer::reset()
{
    for (int i = 0; i < FxController::NUM_SPECTRUM_BANDS * NUM_BARS; i++)
    {
        band_graph_.set(i, 0);
    }
}

void FxVisualizer::update()
{
    updateMeters();
    if (!isEnabled())
        return;

    FxController::getInstance().getSpectrumBandValues(band_values_);

    for (int i = 0; i < FxController::NUM_SPECTRUM_BANDS; i++)
    {
        if (band_values_[i] < 0 || band_values_[i] > 1)
        {
            band_values_.set(i, 0);
        }

        for (int j = 0; j < NUM_BARS / 2; j++)
        {
            band_graph_.set(i*NUM_BARS + j, band_graph_[i*NUM_BARS + j + 1]);
            band_graph_.set(i*NUM_BARS + (NUM_BARS - 1) - j, band_graph_[i*NUM_BARS + j + 1]);
        }

        band_graph_.set(i*NUM_BARS + NUM_BARS / 2, band_values_[i]);
    }
}

void FxVisualizer::paint(Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setFillType(FillType(Colour(FXCOLOR(ControlBackground)).withAlpha(1.0f)));
    g.fillRoundedRectangle(bounds.toFloat(), 8);

    g.setGradientFill(gradient_);

    // ------------------------------------------------------ SPECTRUM AREA - LEFT AND SIZE 
    Path barsPath;

    float x = 27;
    float dx = 9.1;

    for (auto i = 0; i < FxController::NUM_SPECTRUM_BANDS * NUM_BARS; i++)
    {
        float band_value = band_graph_[i] == 0.0 ? 0.01 : band_graph_[i];
        float height = band_value * 54.0f;

        barsPath.addRectangle(x, 35.0f - height / 2.0f, 4.0f, height);
        x += dx;
    }

    g.fillPath(barsPath);

    const bool russian = meterInRussian();
    const String labels[] = {
        russian ? String(L"\u0412\u0445\u043e\u0434") : String("Input"),
        russian ? String(L"\u041f\u043e\u0441\u043b\u0435 EQ") : String("After EQ"),
        russian ? String(L"\u0412\u044b\u0445\u043e\u0434") : String("Output")
    };
    const int cell_width = (getWidth() - 220) / 3;
    g.setFont(11.0f);
    for (int i = 0; i < DfxAudioMeter::StageCount; ++i)
    {
        const auto& reading = meter_snapshot_.stages[i];
        const int left = 18 + i * cell_width;
        const int width = cell_width - 16;
        const float db = DfxAudioMeter::decibels(reading.peak);
        const auto colour = reading.peak > 1.0f ? Colour(0xffef5350)
            : db >= -3.0f ? Colour(0xffffc857) : Colour(0xff51cf8d);

        g.setColour(Colour(FXCOLOR(DefaultText)).withAlpha(1.0f));
        g.drawText(labels[i], left, 78, 65, 18, Justification::centredLeft);
        const auto text = meter_snapshot_.available && reading.observed
            ? meterLevel(reading.peak) + " / max " + meterLevel(reading.maximum) + " dBFS" : "--";
        g.drawText(text, left + 65, 78, width - 65, 18, Justification::centredRight);

        g.setColour(Colour(FXCOLOR(DefaultText)).withAlpha(0.12f));
        g.fillRoundedRectangle(static_cast<float>(left), 102.0f, static_cast<float>(width), 6.0f, 2.0f);
        const float proportion = jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (reading.observed && proportion > 0)
        {
            g.setColour(colour);
            g.fillRoundedRectangle(static_cast<float>(left), 102.0f, width * proportion, 6.0f, 2.0f);
        }
        const float held = jlimit(0.0f, 1.0f,
            (DfxAudioMeter::decibels(reading.maximum) + 60.0f) / 60.0f);
        if (held > 0)
        {
            g.setColour(reading.maximum > 1.0f ? Colour(0xffef5350) : Colours::white);
            g.fillRect(static_cast<float>(left) + (width - 1) * held, 100.0f, 1.0f, 10.0f);
        }
    }
}

void FxVisualizer::resized()
{
    clipping_reset_.setBounds(getWidth() - 194, 79, 176, 31);
}

#if JUCE_MAJOR_VERSION >= 8
void FxVisualizer::timerCallback()
{
    if (isShowing() && !FxController::getInstance().isAudioProcessing())
    {
        updateMeters();
        repaint();
    }
}
#endif

void FxVisualizer::updateMeters()
{
    meter_snapshot_ = FxController::getInstance().getAudioMeter();
    const auto now = Time::getMillisecondCounterHiRes();
    if (meter_snapshot_.blocks != last_meter_block_)
    {
        last_meter_block_ = meter_snapshot_.blocks;
        last_meter_change_ms_ = now;
    }
    if (now - last_meter_change_ms_ > 250.0)
        for (auto& reading : meter_snapshot_.stages)
            reading.peak = 0;

    std::uint64_t overs = 0;
    for (const auto& reading : meter_snapshot_.stages)
        overs += reading.overSamples + reading.invalidSamples;
    const auto reset_label = meterInRussian()
        ? String(L"\u0421\u0431\u0440\u043e\u0441") : String("Reset");
    clipping_reset_.setButtonText("OVER: " + String(static_cast<int64>(overs)) + "   " + reset_label);
    clipping_reset_.setColour(TextButton::buttonColourId,
        overs > 0 ? Colour(0xff9f2838) : Colour(0xff23463a));
    clipping_reset_.setColour(TextButton::textColourOffId, Colours::white);
}

void FxVisualizer::enablementChanged()
{
    if (isEnabled())
    {
        start();
    }
    else
    {
        pause();
        reset();
    }
}

void FxVisualizer::lookAndFeelChanged()
{
    calcGradient();
    repaint();
}

void FxVisualizer::calcGradient()
{
    const bool active = FxModel::getModel().getPowerState();
    float alpha = 0.75;
    if (FxController::getInstance().isAudioProcessing())
    {
        alpha = 1.0;
    }

    gradient_ = ColourGradient(active ? Colour(FXCOLOR(GraphHigh)).withAlpha(alpha) : Colour(FXCOLOR(GraphHigh)).withSaturation(0.0f).withAlpha(alpha),
        2.0f, 0.0f,
        active ? Colour(FXCOLOR(GraphHigh)).withAlpha(alpha) : Colour(FXCOLOR(GraphHigh)).withSaturation(0.0f).withAlpha(alpha),
        2.0f, 100.0f, false);
    if (active)
    {
        gradient_.addColour(0.5f, Colour(FXCOLOR(GraphLow)).withAlpha(alpha));
    }
    else
    {
        gradient_.addColour(0.5f, Colour(FXCOLOR(GraphLow)).withSaturation(0.0f).withAlpha(alpha));
    }
}
