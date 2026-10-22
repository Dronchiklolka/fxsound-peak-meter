// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Dronchiklolka
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

// One audio-thread producer; UI/diagnostic readers only access lock-free atomics.
// This observes samples without changing audio, allocating, or acquiring a lock.
class DfxAudioMeter
{
public:
    enum Stage { Input = 0, AfterEq, Output, StageCount };

    struct Reading
    {
        bool observed = false;
        float peak = 0.0f;
        float maximum = 0.0f;
        std::uint64_t overSamples = 0;
        std::uint64_t fullScaleSamples = 0;
        std::uint64_t invalidSamples = 0;
    };

    struct Snapshot
    {
        std::array<Reading, StageCount> stages{};
        std::uint64_t blocks = 0;
        int channels = 0;
        int sampleRate = 0;
        bool available = false;
    };

    void configure(int bits, int channels, int sampleRate) noexcept
    {
        const int newChannels = channels > 0 && channels <= 8 ? channels : 0;
        const int newSampleRate = sampleRate > 0 ? sampleRate : 0;
        const bool newAvailable = bits == 32 && newChannels > 0 && newSampleRate > 0;
        // AudioPassthru supplies the format on every block. An unchanged format
        // must not erase held peaks or the overload history.
        if (newChannels == channels_ && newSampleRate == sampleRate_
            && newAvailable == available_.load(std::memory_order_relaxed))
            return;
        channels_ = newChannels;
        sampleRate_ = newSampleRate;
        observing_ = false;
        available_.store(newAvailable, std::memory_order_relaxed);
        publishedChannels_.store(channels_, std::memory_order_relaxed);
        publishedSampleRate_.store(sampleRate_, std::memory_order_relaxed);
        requestReset();
    }

    bool beginBlock(int frames) noexcept
    {
        observing_ = false;
        if (frames <= 0 || !available_.load(std::memory_order_relaxed))
            return false;

        const auto generation = resetGeneration_.load(std::memory_order_acquire);
        if (generation != appliedGeneration_)
        {
            for (auto& stage : running_)
                stage = RunningStage{};
            appliedGeneration_ = generation;
        }
        for (auto& block : block_)
            block = Reading{};

        // 24 dB/second falloff; instantaneous attack catches short peaks.
        decay_ = std::pow(10.0f, -1.2f * static_cast<float>(frames)
                         / static_cast<float>(sampleRate_));
        observing_ = true;
        return true;
    }

    void observe(Stage stage, const float* samples, std::size_t count) noexcept
    {
        if (!observing_ || samples == nullptr || count == 0 || stage < Input || stage >= StageCount)
            return;
        auto& block = block_[stage];
        block.observed = true;
        for (std::size_t i = 0; i < count; ++i)
        {
            const float value = samples[i];
            if (!std::isfinite(value))
            {
                ++block.invalidSamples;
                continue;
            }
            const float magnitude = std::fabs(value);
            if (magnitude > block.peak)
                block.peak = magnitude;
            // Exactly +/-1 is full scale, not proof that a sample was clipped.
            if (magnitude == 1.0f)
                ++block.fullScaleSamples;
            if (magnitude > 1.0f)
                ++block.overSamples;
        }
    }

    void endBlock() noexcept
    {
        if (!observing_)
            return;
        observing_ = false;
        for (int i = 0; i < StageCount; ++i)
        {
            auto& running = running_[i];
            const auto& block = block_[i];
            running.observed = block.observed;
            const float falling = running.peak * decay_;
            running.peak = block.peak > falling ? block.peak : falling;
            if (block.peak > running.maximum)
                running.maximum = block.peak;
            running.overSamples += block.overSamples;
            running.fullScaleSamples += block.fullScaleSamples;
            running.invalidSamples += block.invalidSamples;

            auto& published = published_[i];
            published.observed.store(running.observed, std::memory_order_relaxed);
            published.peak.store(running.peak, std::memory_order_relaxed);
            published.maximum.store(running.maximum, std::memory_order_relaxed);
            published.overSamples.store(running.overSamples, std::memory_order_relaxed);
            published.fullScaleSamples.store(running.fullScaleSamples, std::memory_order_relaxed);
            published.invalidSamples.store(running.invalidSamples, std::memory_order_relaxed);
        }
        publishedGeneration_.store(appliedGeneration_, std::memory_order_release);
        publishedBlocks_.store(++blocks_, std::memory_order_release);
    }

    void requestReset() noexcept
    {
        resetGeneration_.fetch_add(1, std::memory_order_release);
    }

    Snapshot snapshot() const noexcept
    {
        Snapshot result;
        result.blocks = publishedBlocks_.load(std::memory_order_acquire);
        result.available = available_.load(std::memory_order_relaxed);
        result.channels = publishedChannels_.load(std::memory_order_relaxed);
        result.sampleRate = publishedSampleRate_.load(std::memory_order_relaxed);
        const auto requested = resetGeneration_.load(std::memory_order_acquire);
        if (requested != publishedGeneration_.load(std::memory_order_acquire))
            return result; // Clear immediately, including while audio is stopped.

        for (int i = 0; i < StageCount; ++i)
        {
            const auto& published = published_[i];
            auto& reading = result.stages[i];
            reading.observed = published.observed.load(std::memory_order_relaxed);
            reading.peak = published.peak.load(std::memory_order_relaxed);
            reading.maximum = published.maximum.load(std::memory_order_relaxed);
            reading.overSamples = published.overSamples.load(std::memory_order_relaxed);
            reading.fullScaleSamples = published.fullScaleSamples.load(std::memory_order_relaxed);
            reading.invalidSamples = published.invalidSamples.load(std::memory_order_relaxed);
        }
        return result;
    }

    static float decibels(float magnitude) noexcept
    {
        return magnitude > 0.000001f && std::isfinite(magnitude)
            ? 20.0f * std::log10(magnitude) : -120.0f;
    }

private:
    static_assert(ATOMIC_INT_LOCK_FREE == 2 && ATOMIC_LLONG_LOCK_FREE == 2,
                  "Meter counters must be lock-free on the target platform");

    struct RunningStage : Reading {};
    struct PublishedStage
    {
        std::atomic<bool> observed{false};
        std::atomic<float> peak{0.0f}, maximum{0.0f};
        std::atomic<std::uint64_t> overSamples{0}, fullScaleSamples{0}, invalidSamples{0};
    };

    std::array<Reading, StageCount> block_{};
    std::array<RunningStage, StageCount> running_{};
    std::array<PublishedStage, StageCount> published_{};
    std::atomic<std::uint32_t> resetGeneration_{0}, publishedGeneration_{0};
    std::atomic<std::uint64_t> publishedBlocks_{0};
    std::atomic<int> publishedChannels_{0}, publishedSampleRate_{0};
    std::atomic<bool> available_{false};
    std::uint32_t appliedGeneration_ = 0;
    std::uint64_t blocks_ = 0;
    int channels_ = 0, sampleRate_ = 0;
    float decay_ = 1.0f;
    bool observing_ = false;
};
