#include "ScDiscAudioEngine.h"

#include <chrono>
#include <cmath>
#include <iostream>

namespace
{
bool isFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample))) return false;
    return true;
}
}

int main()
{
    struct Format { double sampleRate; int blockSize; int inputChannels; };
    const Format formats[] {
        { 44100.0, 64, 0 }, { 48000.0, 128, 2 }, { 48000.0, 512, 0 },
        { 96000.0, 256, 2 }, { 44100.0, 2048, 0 }, { 0.0, 0, 0 }
    };

    ScDiscAudioEngine engine;
    double worstRealtimeRatio = 0.0;
    float observedPeak = 0.0f;

    for (const auto format : formats)
    {
        const auto rate = format.sampleRate > 0.0 ? format.sampleRate : 44100.0;
        const auto block = format.blockSize > 0 ? format.blockSize : 64;
        engine.prepare (format.sampleRate, format.blockSize, 2, format.inputChannels);
        if (! engine.isReady())
        {
            std::cerr << "Audio engine failed to prepare: " << engine.getStatusText() << '\n';
            return 1;
        }

        engine.setPdAudioInputMuted (true);
        if (! engine.isPdAudioInputMuted()) return 2;

        juce::AudioBuffer<float> output (2, block);
        juce::AudioBuffer<float> input (juce::jmax (1, format.inputChannels), block);
        input.clear();
        for (int sample = 0; sample < block; ++sample)
            input.setSample (0, sample, std::sin ((float) sample * 0.031f) * 0.1f);

        for (int note = 0; note < 96; ++note)
            engine.triggerMidiNote (36 + note % 48, 0.18f);

        constexpr int renderBlocks = 320;
        const auto started = std::chrono::steady_clock::now();
        for (int i = 0; i < renderBlocks; ++i)
        {
            if ((i % 16) == 0)
                for (int note = 0; note < 24; ++note)
                    engine.triggerMidiNote (48 + note % 24, 0.12f);
            engine.render (output, &input);
            if (! isFinite (output))
            {
                std::cerr << "Audio engine produced a non-finite sample\n";
                return 3;
            }
            observedPeak = juce::jmax (observedPeak, output.getMagnitude (0, output.getNumSamples()));
        }
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
        const auto renderedDuration = (double) renderBlocks * block / rate;
        worstRealtimeRatio = juce::jmax (worstRealtimeRatio, elapsed / renderedDuration);

        engine.setPdAudioInputMuted (false);
        if (engine.isPdAudioInputMuted()) return 4;
        engine.render (output, &input);
        if (! isFinite (output)) return 5;
        engine.release();
    }

    if (observedPeak <= 0.0001f)
    {
        std::cerr << "Stress render remained silent\n";
        return 6;
    }
    if (worstRealtimeRatio >= 1.5)
    {
        std::cerr << "Stress render missed real-time: ratio=" << worstRealtimeRatio << '\n';
        return 7;
    }

    std::cout << "audioFormats=" << std::size (formats)
              << " peak=" << observedPeak
              << " worstRealtimeRatio=" << worstRealtimeRatio << '\n';
    return 0;
}
