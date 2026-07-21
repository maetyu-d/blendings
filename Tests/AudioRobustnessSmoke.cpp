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

    engine.prepare (48000.0, 512, 2, 0);
    if (! engine.isReady()) return 8;
    engine.setTempo (120.0);
    juce::AudioBuffer<float> scheduledOutput (2, 100);
    DiscAudioTrigger scheduledTone;
    scheduledTone.soundElementCount = 1;
    const auto dueSample = engine.alignedEventSample (128);
    engine.scheduleTriggerManyAtSample ({ scheduledTone }, dueSample);

    engine.render (scheduledOutput);
    if (scheduledOutput.getMagnitude (0, scheduledOutput.getNumSamples()) > 0.00001f)
    {
        std::cerr << "Scheduled event sounded before its sample position\n";
        return 9;
    }
    if (engine.getRenderedSamplePosition() != 100)
    {
        std::cerr << "Audio sample clock drifted across a non-quantum block\n";
        return 10;
    }

    engine.suspendScheduledEvents();
    engine.render (scheduledOutput);
    if (scheduledOutput.getMagnitude (0, scheduledOutput.getNumSamples()) > 0.00001f)
    {
        std::cerr << "Suspended event sounded while transport was paused\n";
        return 11;
    }
    engine.render (scheduledOutput);
    if (scheduledOutput.getMagnitude (0, scheduledOutput.getNumSamples()) > 0.00001f)
    {
        std::cerr << "Suspended event escaped during a long pause\n";
        return 12;
    }
    engine.resumeScheduledEvents();

    engine.render (scheduledOutput);
    if (scheduledOutput.getMagnitude (0, 84) > 0.00001f)
    {
        std::cerr << "Resumed event crossed its shifted in-buffer boundary\n";
        return 13;
    }

    auto scheduledPeak = scheduledOutput.getMagnitude (0, 84, scheduledOutput.getNumSamples() - 84);
    for (int block = 0; block < 8; ++block)
    {
        engine.render (scheduledOutput);
        scheduledPeak = juce::jmax (scheduledPeak, scheduledOutput.getMagnitude (0, scheduledOutput.getNumSamples()));
    }
    if (scheduledPeak <= 0.0001f)
    {
        std::cerr << "Scheduled event did not sound at or after its sample position\n";
        return 14;
    }

    engine.cancelScheduledEvents();
    auto drainedTailPeak = 0.0f;
    for (int block = 0; block < 48; ++block)
    {
        engine.render (scheduledOutput);
        if (block >= 2)
            drainedTailPeak = juce::jmax (drainedTailPeak, scheduledOutput.getMagnitude (0, scheduledOutput.getNumSamples()));
    }
    if (drainedTailPeak <= 0.000001f)
    {
        std::cerr << "Cancelling future events cut off an active release tail\n";
        return 15;
    }
    engine.stopPreview();
    engine.release();

    engine.prepare (48000.0, 512, 2, 0);
    if (! engine.isReady()) return 16;
    DiscAudioTrigger pdTail;
    pdTail.pdDurationSeconds = 0.05f;
    pdTail.pdPatch =
        "#N canvas 120 120 520 360 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 112 0 \\, 0.3 5 \\, 0 450 5;\n"
        "#X obj 48 148 vline~;\n"
        "#X obj 164 112 osc~ 330;\n"
        "#X obj 164 148 *~;\n"
        "#X obj 164 188 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";
    engine.trigger (pdTail);
    auto pdTailPeak = 0.0f;
    for (int block = 0; block < 120; ++block)
    {
        engine.render (scheduledOutput);
        if (block >= 48)
            pdTailPeak = juce::jmax (pdTailPeak, scheduledOutput.getMagnitude (0, scheduledOutput.getNumSamples()));
    }
    if (pdTailPeak <= 0.0001f)
    {
        std::cerr << "Pd duration cut off a still-audible release tail\n";
        return 17;
    }
    engine.stopPreview();
    engine.release();

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
