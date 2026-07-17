#include "ScDiscAudioEngine.h"
#include "GridModel.h"
#include "ScSheetScore.h"

#include <cstdlib>
#include <cmath>
#include <iostream>

static bool writeMonoWavFile (const juce::File& file)
{
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr || stream->failedToOpen())
        return false;

    constexpr int sampleRate = 44100;
    constexpr int sampleCount = 2048;
    constexpr int bytesPerSample = 2;
    constexpr int dataBytes = sampleCount * bytesPerSample;
    constexpr int riffBytes = 36 + dataBytes;

    const auto writeU16 = [&] (int value)
    {
        const unsigned char bytes[] {
            static_cast<unsigned char> (value & 0xff),
            static_cast<unsigned char> ((value >> 8) & 0xff)
        };
        stream->write (bytes, 2);
    };

    const auto writeU32 = [&] (int value)
    {
        const unsigned char bytes[] {
            static_cast<unsigned char> (value & 0xff),
            static_cast<unsigned char> ((value >> 8) & 0xff),
            static_cast<unsigned char> ((value >> 16) & 0xff),
            static_cast<unsigned char> ((value >> 24) & 0xff)
        };
        stream->write (bytes, 4);
    };

    stream->write ("RIFF", 4);
    writeU32 (riffBytes);
    stream->write ("WAVE", 4);
    stream->write ("fmt ", 4);
    writeU32 (16);
    writeU16 (1);
    writeU16 (1);
    writeU32 (sampleRate);
    writeU32 (sampleRate * bytesPerSample);
    writeU16 (bytesPerSample);
    writeU16 (16);
    stream->write ("data", 4);
    writeU32 (dataBytes);

    for (int i = 0; i < sampleCount; ++i)
    {
        const auto phase = static_cast<float> (i) / static_cast<float> (sampleRate);
        const auto sample = std::sin (juce::MathConstants<float>::twoPi * 330.0f * phase) * 0.7f;
        writeU16 (static_cast<short> (juce::roundToInt (sample * 32767.0f)));
    }

    return ! stream->getStatus().failed();
}

static float readPcm16WavPeak (const juce::File& file)
{
    std::unique_ptr<juce::FileInputStream> stream (file.createInputStream());

    if (stream == nullptr || stream->failedToOpen() || stream->getTotalLength() < 44)
        return 0.0f;

    stream->setPosition (0);
    juce::MemoryBlock bytes;
    stream->readIntoMemoryBlock (bytes);

    const auto* data = static_cast<const unsigned char*> (bytes.getData());
    const auto size = bytes.getSize();

    auto readU16 = [&] (size_t offset) -> int
    {
        if (offset + 1 >= size)
            return 0;

        return static_cast<int> (data[offset]) | (static_cast<int> (data[offset + 1]) << 8);
    };

    auto readU32 = [&] (size_t offset) -> int
    {
        if (offset + 3 >= size)
            return 0;

        return static_cast<int> (data[offset])
             | (static_cast<int> (data[offset + 1]) << 8)
             | (static_cast<int> (data[offset + 2]) << 16)
             | (static_cast<int> (data[offset + 3]) << 24);
    };

    size_t offset = 12;
    auto dataOffset = static_cast<size_t> (0);
    auto dataBytes = 0;
    auto bitsPerSample = 0;

    while (offset + 8 <= size)
    {
        const auto chunkSize = readU32 (offset + 4);
        const auto nextOffset = offset + 8 + static_cast<size_t> (chunkSize) + static_cast<size_t> (chunkSize & 1);

        if (std::memcmp (data + offset, "fmt ", 4) == 0 && offset + 24 <= size)
            bitsPerSample = readU16 (offset + 22);
        else if (std::memcmp (data + offset, "data", 4) == 0)
        {
            dataOffset = offset + 8;
            dataBytes = chunkSize;
            break;
        }

        if (nextOffset <= offset || nextOffset > size)
            break;

        offset = nextOffset;
    }

    if (dataOffset == 0 || dataBytes <= 0 || bitsPerSample != 16)
        return 0.0f;

    auto peak = 0.0f;
    const auto end = std::min (dataOffset + static_cast<size_t> (dataBytes), size);

    for (auto i = dataOffset; i + 1 < end; i += 2)
    {
        const auto sample = static_cast<short> (readU16 (i));
        peak = std::max (peak, std::abs (static_cast<float> (sample) / 32768.0f));
    }

    return peak;
}

int main()
{
    std::cout << std::unitbuf;

    ScDiscAudioEngine engine;
    engine.prepare (44100.0, 512, 2);

    std::cout << engine.getStatusText() << '\n';

    if (! engine.isReady())
        return 2;

    DiscAudioTrigger soundTrigger;
    soundTrigger.soundElementCount = 4;
    engine.trigger (soundTrigger);

    DiscAudioTrigger nestedTrigger;
    nestedTrigger.soundElementCount = 1;
    nestedTrigger.nestedWorldPulse = true;
    nestedTrigger.depth = 1;
    nestedTrigger.branchIndex = 1;
    engine.trigger (nestedTrigger);

    DiscAudioTrigger scCodeTrigger;
    scCodeTrigger.depth = 1;
    scCodeTrigger.branchIndex = 2;
    scCodeTrigger.midiNote = 64;
    scCodeTrigger.scDurationSeconds = -1.0f;
    scCodeTrigger.scCode =
        "var hold = Select.kr(sustain < 0, [sustain.max(0.25), 1.0]);\n"
        "var env = EnvGen.kr(Env.linen(0.01, hold, 0.2), doneAction: 2);\n"
        "var tone = SinOsc.ar([pitch.midicps, (pitch + 7).midicps]) * env * amp;\n"
        "Out.ar(out, Pan2.ar(tone.sum.tanh, pan));";
    engine.trigger (scCodeTrigger);

    DiscAudioTrigger pdTrigger;
    pdTrigger.depth = 1;
    pdTrigger.branchIndex = 5;
    pdTrigger.midiNote = 69;
    pdTrigger.pdDurationSeconds = 0.45f;
    pdTrigger.pdPatch =
        "#N canvas 120 120 520 360 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 112 0 \\, 0.3 5 \\, 0 450 5;\n"
        "#X obj 48 148 vline~;\n"
        "#X obj 164 112 osc~ 330;\n"
        "#X obj 164 148 *~;\n"
        "#X obj 164 188 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";
    engine.trigger (pdTrigger);

    DiscAudioTrigger pdBundledExtraTrigger;
    pdBundledExtraTrigger.depth = 1;
    pdBundledExtraTrigger.branchIndex = 6;
    pdBundledExtraTrigger.pdDurationSeconds = 0.35f;
    pdBundledExtraTrigger.pdPatch =
        "#N canvas 140 140 560 360 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 112 0 \\, 0.2 5 \\, 0 300 5;\n"
        "#X obj 48 148 vline~;\n"
        "#X obj 164 112 osc~ 440;\n"
        "#X obj 164 148 *~;\n"
        "#X obj 164 188 rev3~ 94 88 3000 20;\n"
        "#X obj 164 238 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 1 6 1;\n";
    engine.trigger (pdBundledExtraTrigger);

    DiscAudioTrigger pdDeclaredLibTrigger;
    pdDeclaredLibTrigger.depth = 1;
    pdDeclaredLibTrigger.branchIndex = 7;
    pdDeclaredLibTrigger.pdDurationSeconds = 0.2f;
    pdDeclaredLibTrigger.pdPatch =
        "#N canvas 140 140 460 260 10;\n"
        "#X declare -stdlib bob~ -lib definitely_missing_otherware_lib;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 92 0 \\, 0.15 5 \\, 0 180 5;\n"
        "#X obj 48 128 vline~;\n"
        "#X obj 164 92 osc~ 550;\n"
        "#X obj 164 128 *~;\n"
        "#X obj 164 168 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";
    engine.trigger (pdDeclaredLibTrigger);

    const auto pdDeclareStatus = engine.getStatusText();

    if (! pdDeclareStatus.contains ("unsupported Pd libs: definitely_missing_otherware_lib"))
    {
        std::cerr << "missing Pd declare -lib warning in status: "
                  << pdDeclareStatus << '\n';
        return 4;
    }

    if (pdDeclareStatus.contains ("unsupported Pd libs: bob~")
        || pdDeclareStatus.contains ("bob~, definitely_missing_otherware_lib"))
    {
        std::cerr << "bundled Pd declare -stdlib library was incorrectly reported unsupported: "
                  << pdDeclareStatus << '\n';
        return 4;
    }

    DiscAudioTrigger pdBundledAbstractionLibTrigger;
    pdBundledAbstractionLibTrigger.pdPatch =
        "#N canvas 40 40 320 180 10;\n"
        "#X declare -stdlib rev3~ -lib hilbert~ -lib complex-mod~ -lib output~;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 bang~;\n";
    engine.trigger (pdBundledAbstractionLibTrigger);

    const auto pdBundledAbstractionLibStatus = engine.getStatusText();
    if (pdBundledAbstractionLibStatus.contains ("unsupported Pd libs: rev3~")
        || pdBundledAbstractionLibStatus.contains ("hilbert~")
        || pdBundledAbstractionLibStatus.contains ("complex-mod~")
        || pdBundledAbstractionLibStatus.contains ("output~"))
    {
        std::cerr << "bundled Pd abstraction libraries were incorrectly reported unsupported: "
                  << pdBundledAbstractionLibStatus << '\n';
        return 4;
    }

    DiscAudioTrigger pdFalseDeclareTrigger;
    pdFalseDeclareTrigger.pdPatch =
        "#N canvas 40 40 320 180 10;\n"
        "#X obj 48 42 symbol declare -lib definitely_missing_otherware_false_positive;\n";
    engine.trigger (pdFalseDeclareTrigger);

    const auto pdFalseDeclareStatus = engine.getStatusText();
    if (pdFalseDeclareStatus.contains ("definitely_missing_otherware_false_positive"))
    {
        std::cerr << "ordinary Pd object argument was incorrectly parsed as a declare -lib: "
                  << pdFalseDeclareStatus << '\n';
        return 4;
    }

    const auto abstractionDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                          .getNonexistentChildFile ("otherware-pd-abstraction-smoke", {}, false);
    abstractionDirectory.createDirectory();
    abstractionDirectory.getChildFile ("otherwarevoice.pd").replaceWithText (
        "#N canvas 80 80 360 260 10;\n"
        "#X obj 35 35 inlet;\n"
        "#X msg 35 76 0 \\, 0.18 5 \\, 0 260 5;\n"
        "#X obj 35 112 vline~;\n"
        "#X obj 145 76 osc~ 660;\n"
        "#X obj 145 112 *~;\n"
        "#X obj 145 152 outlet~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n");

    DiscAudioTrigger pdAbstractionTrigger;
    pdAbstractionTrigger.depth = 1;
    pdAbstractionTrigger.branchIndex = 8;
    pdAbstractionTrigger.pdDurationSeconds = 0.35f;
    pdAbstractionTrigger.pdSearchPath = abstractionDirectory.getFullPathName();
    pdAbstractionTrigger.pdPatch =
        "#N canvas 120 120 420 260 10;\n"
        "#X obj 46 42 r trigger;\n"
        "#X obj 46 92 otherwarevoice;\n"
        "#X obj 46 142 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 2 1;\n";
    engine.trigger (pdAbstractionTrigger);

    ScoreDocument sheet;
    sheet.addDefaultSection();
    sheet.getSection (0).bars = 2.0;
    sheet.addDefaultRow();
    auto& row = sheet.getRow (0);
    row.name = "Smoke sheet pulse";
    row.group = "test";
    row.voice = VoiceType::tone;
    row.bars = 2.0;
    row.stepBeats = 0.5;
    row.noteBeats = 0.2;
    row.density = 1.0;
    row.amp = 0.22f;
    row.pattern = PatternType::euclidean;
    row.contour = ContourType::arch;

    DiscAudioTrigger sheetTrigger;
    sheetTrigger.depth = 1;
    sheetTrigger.branchIndex = 3;
    sheetTrigger.hasScSheet = true;
    sheetTrigger.scSheet = sheet.toValueTree();
    engine.trigger (sheetTrigger);

    gridcollider::GridModel orcaGrid (16, 8);
    const juce::StringArray orcaRows {
        "..1D4...........",
        "..*:05Cf4.......",
        "................",
        "..2D8...........",
        "..*:15Ge6.......",
        "................",
        "................",
        "................"
    };

    for (int y = 0; y < orcaRows.size(); ++y)
        for (int x = 0; x < orcaRows[y].length(); ++x)
            orcaGrid.setGlyph (x, y, static_cast<char> (orcaRows[y][x]));

    DiscAudioTrigger orcaTrigger;
    orcaTrigger.depth = 1;
    orcaTrigger.branchIndex = 4;
    orcaTrigger.hasOrcaGrid = true;
    orcaTrigger.orcaGrid = orcaGrid.createSnapshot();
    engine.trigger (orcaTrigger);

    juce::AudioBuffer<float> buffer (2, 512);
    auto peak = 0.0f;
    auto sumSquares = 0.0;
    auto sampleCount = 0;

    for (int block = 0; block < 160; ++block)
    {
        engine.render (buffer);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* samples = buffer.getReadPointer (channel);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto sample = samples[i];
                peak = std::max (peak, std::abs (sample));
                sumSquares += static_cast<double> (sample) * static_cast<double> (sample);
                ++sampleCount;
            }
        }
    }

    const auto rms = std::sqrt (sumSquares / static_cast<double> (sampleCount));
    std::cout << "peak=" << peak << " rms=" << rms << '\n';

    engine.release();

    if (peak <= 0.0005f)
        return 3;

    ScDiscAudioEngine abstractionOnlyEngine;
    abstractionOnlyEngine.prepare (44100.0, 512, 2);
    abstractionOnlyEngine.trigger (pdAbstractionTrigger);

    juce::AudioBuffer<float> abstractionBuffer (2, 512);
    auto abstractionPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        abstractionOnlyEngine.render (abstractionBuffer);

        for (int channel = 0; channel < abstractionBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = abstractionBuffer.getReadPointer (channel);

            for (int i = 0; i < abstractionBuffer.getNumSamples(); ++i)
                abstractionPeak = std::max (abstractionPeak, std::abs (samples[i]));
        }
    }

    abstractionOnlyEngine.release();

    if (abstractionPeak <= 0.0005f)
    {
        std::cerr << "Pd project abstraction produced no audio; peak="
                  << abstractionPeak << '\n';
        return 5;
    }

    std::cout << "abstractionPeak=" << abstractionPeak << '\n';

    DiscAudioTrigger pdBundledExtrasTrigger;
    pdBundledExtrasTrigger.depth = 1;
    pdBundledExtrasTrigger.branchIndex = 39;
    pdBundledExtrasTrigger.pdDurationSeconds = 0.35f;
    pdBundledExtrasTrigger.pdPatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 160 122 clear \\, add 1 0 0 \\, add 0 1 0 \\, add 0 0 1;\n"
        "#X msg 48 122 0.8 0.1 0.1;\n"
        "#X obj 48 162 choice;\n"
        "#X obj 48 202 + 62;\n"
        "#X obj 48 242 mtof;\n"
        "#X obj 48 282 sig~;\n"
        "#X obj 48 322 osc~;\n"
        "#X obj 184 322 hilbert~;\n"
        "#X obj 360 282 sig~ 90;\n"
        "#X obj 184 382 complex-mod~;\n"
        "#X obj 184 422 lrshift~ 1;\n"
        "#X obj 500 282 vline~;\n"
        "#X msg 500 242 0 \\, 0.22 5 \\, 0 260 5;\n"
        "#X obj 184 462 *~;\n"
        "#X obj 184 502 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 3 0;\n"
        "#X connect 1 0 14 0;\n"
        "#X connect 1 1 2 0;\n"
        "#X connect 2 0 4 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 11 0;\n"
        "#X connect 9 1 11 1;\n"
        "#X connect 10 0 11 2;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 15 0;\n"
        "#X connect 13 0 15 1;\n"
        "#X connect 14 0 13 0;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 15 0 16 1;\n";

    ScDiscAudioEngine bundledExtrasEngine;
    bundledExtrasEngine.prepare (44100.0, 512, 2);
    bundledExtrasEngine.trigger (pdBundledExtrasTrigger);

    juce::AudioBuffer<float> bundledExtrasBuffer (2, 512);
    auto bundledExtrasPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        bundledExtrasEngine.render (bundledExtrasBuffer);

        for (int channel = 0; channel < bundledExtrasBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = bundledExtrasBuffer.getReadPointer (channel);

            for (int i = 0; i < bundledExtrasBuffer.getNumSamples(); ++i)
                bundledExtrasPeak = std::max (bundledExtrasPeak, std::abs (samples[i]));
        }
    }

    bundledExtrasEngine.release();

    if (bundledExtrasPeak <= 0.0005f)
    {
        std::cerr << "Pd bundled choice/hilbert~/complex-mod~/lrshift~ fixture produced no audio; peak="
                  << bundledExtrasPeak << '\n';
        return 45;
    }

    std::cout << "bundledExtrasPeak=" << bundledExtrasPeak << '\n';

    DiscAudioTrigger pdAnalysisExtrasTrigger;
    pdAnalysisExtrasTrigger.depth = 1;
    pdAnalysisExtrasTrigger.branchIndex = 40;
    pdAnalysisExtrasTrigger.pdDurationSeconds = 0.35f;
    pdAnalysisExtrasTrigger.pdPatch =
        "#N canvas 120 120 780 560 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 48 122 0 \\, 0.2 5 \\, 0 260 5;\n"
        "#X obj 48 162 vline~;\n"
        "#X obj 188 122 osc~ 440;\n"
        "#X obj 188 162 *~;\n"
        "#X obj 188 202 rev1~;\n"
        "#X obj 188 242 rev2~;\n"
        "#X obj 188 282 *~ 0.35;\n"
        "#X obj 188 322 dac~;\n"
        "#X obj 420 162 bonk~;\n"
        "#X obj 420 202 fiddle~ 1024 1 20 3;\n"
        "#X obj 420 242 sigmund~ pitch env;\n"
        "#X obj 420 282 pique;\n"
        "#X obj 570 162 loop~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 8 0;\n"
        "#X connect 5 0 10 0;\n"
        "#X connect 5 0 11 0;\n"
        "#X connect 5 0 12 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine analysisExtrasEngine;
    analysisExtrasEngine.prepare (44100.0, 512, 2);
    analysisExtrasEngine.trigger (pdAnalysisExtrasTrigger);

    juce::AudioBuffer<float> analysisExtrasBuffer (2, 512);
    auto analysisExtrasPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        analysisExtrasEngine.render (analysisExtrasBuffer);

        for (int channel = 0; channel < analysisExtrasBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = analysisExtrasBuffer.getReadPointer (channel);

            for (int i = 0; i < analysisExtrasBuffer.getNumSamples(); ++i)
                analysisExtrasPeak = std::max (analysisExtrasPeak, std::abs (samples[i]));
        }
    }

    analysisExtrasEngine.release();

    if (analysisExtrasPeak <= 0.0005f)
    {
        std::cerr << "Pd bundled bonk~/fiddle~/sigmund~/pique/loop~/rev1~/rev2~ fixture produced no audio; peak="
                  << analysisExtrasPeak << '\n';
        return 46;
    }

    std::cout << "analysisExtrasPeak=" << analysisExtrasPeak << '\n';

    const auto escapedPathDirectory = abstractionDirectory.getChildFile ("pd voices");
    escapedPathDirectory.createDirectory();
    escapedPathDirectory.getChildFile ("escapedvoice.pd").replaceWithText (
        "#N canvas 80 80 360 260 10;\n"
        "#X obj 35 35 inlet;\n"
        "#X msg 35 76 0 \\, 0.16 5 \\, 0 230 5;\n"
        "#X obj 35 112 vline~;\n"
        "#X obj 145 76 osc~ 845;\n"
        "#X obj 145 112 *~;\n"
        "#X obj 145 152 outlet~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n");

    DiscAudioTrigger pdEscapedPathTrigger;
    pdEscapedPathTrigger.depth = 1;
    pdEscapedPathTrigger.branchIndex = 13;
    pdEscapedPathTrigger.pdDurationSeconds = 0.3f;
    pdEscapedPathTrigger.pdSearchPath = abstractionDirectory.getFullPathName();
    pdEscapedPathTrigger.pdPatch =
        "#N canvas 120 120 440 260 10;\n"
        "#X declare -path pd\\ voices, f 20;\n"
        "#X obj 46 42 r trigger;\n"
        "#X obj 46 92 escapedvoice;\n"
        "#X obj 46 142 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 2 1;\n";

    ScDiscAudioEngine escapedPathEngine;
    escapedPathEngine.prepare (44100.0, 512, 2);
    escapedPathEngine.trigger (pdEscapedPathTrigger);

    juce::AudioBuffer<float> escapedPathBuffer (2, 512);
    auto escapedPathPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        escapedPathEngine.render (escapedPathBuffer);

        for (int channel = 0; channel < escapedPathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = escapedPathBuffer.getReadPointer (channel);

            for (int i = 0; i < escapedPathBuffer.getNumSamples(); ++i)
                escapedPathPeak = std::max (escapedPathPeak, std::abs (samples[i]));
        }
    }

    escapedPathEngine.release();

    const auto standardPathDirectory = juce::File (juce::String (OTHERWARE_PD_EXTRA_ROOT).unquoted())
                                           .getParentDirectory()
                                           .getChildFile ("otherware-stdpath-smoke");
    standardPathDirectory.deleteRecursively();
    standardPathDirectory.createDirectory();
    const auto stdPathAbstraction = standardPathDirectory.getChildFile ("otherwarestdvoice.pd");

    if (! stdPathAbstraction.replaceWithText (
        "#N canvas 80 80 360 260 10;\n"
        "#X obj 35 35 inlet;\n"
        "#X msg 35 76 0 \\, 0.17 5 \\, 0 240 5;\n"
        "#X obj 35 112 vline~;\n"
        "#X obj 145 76 osc~ 705;\n"
        "#X obj 145 112 *~;\n"
        "#X obj 145 152 outlet~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"))
    {
        abstractionDirectory.deleteRecursively();
        standardPathDirectory.deleteRecursively();
        std::cerr << "Could not write Pd -stdpath smoke abstraction at "
                  << stdPathAbstraction.getFullPathName() << '\n';
        return 108;
    }

    DiscAudioTrigger pdStdPathTrigger;
    pdStdPathTrigger.depth = 1;
    pdStdPathTrigger.branchIndex = 54;
    pdStdPathTrigger.pdDurationSeconds = 0.3f;
    pdStdPathTrigger.pdSearchPath = abstractionDirectory.getFullPathName();
    pdStdPathTrigger.pdPatch =
        "#N canvas 120 120 440 260 10;\n"
        "#X declare -stdpath otherware-stdpath-smoke;\n"
        "#X obj 46 42 r trigger;\n"
        "#X obj 46 92 otherwarestdvoice;\n"
        "#X obj 46 142 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 2 1;\n";

    ScDiscAudioEngine stdPathEngine;
    stdPathEngine.prepare (44100.0, 512, 2);
    stdPathEngine.trigger (pdStdPathTrigger);

    juce::AudioBuffer<float> stdPathBuffer (2, 512);
    auto stdPathPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        stdPathEngine.render (stdPathBuffer);

        for (int channel = 0; channel < stdPathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = stdPathBuffer.getReadPointer (channel);

            for (int i = 0; i < stdPathBuffer.getNumSamples(); ++i)
                stdPathPeak = std::max (stdPathPeak, std::abs (samples[i]));
        }
    }

    if (escapedPathPeak <= 0.0005f)
    {
        std::cerr << "Pd escaped declare -path abstraction produced no audio; peak="
                  << escapedPathPeak << '\n';
        return 18;
    }

    std::cout << "escapedPathPeak=" << escapedPathPeak << '\n';

    if (stdPathPeak <= 0.0005f)
    {
        std::cerr << "Pd declare -stdpath abstraction produced no audio; peak="
                  << stdPathPeak << '\n';
        return 108;
    }

    std::cout << "stdPathPeak=" << stdPathPeak << '\n';

    DiscAudioTrigger pdStdPathNoSearchTrigger;
    pdStdPathNoSearchTrigger.depth = 1;
    pdStdPathNoSearchTrigger.branchIndex = 55;
    pdStdPathNoSearchTrigger.pdDurationSeconds = 0.3f;
    pdStdPathNoSearchTrigger.pdPatch =
        "#N canvas 120 120 440 260 10;\n"
        "#X declare -stdpath otherware-stdpath-smoke;\n"
        "#X obj 46 42 r trigger;\n"
        "#X obj 46 92 otherwarestdvoice;\n"
        "#X obj 46 142 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 2 1;\n";

    ScDiscAudioEngine stdPathNoSearchEngine;
    stdPathNoSearchEngine.prepare (44100.0, 512, 2);
    stdPathNoSearchEngine.trigger (pdStdPathNoSearchTrigger);

    juce::AudioBuffer<float> stdPathNoSearchBuffer (2, 512);
    auto stdPathNoSearchPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        stdPathNoSearchEngine.render (stdPathNoSearchBuffer);

        for (int channel = 0; channel < stdPathNoSearchBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = stdPathNoSearchBuffer.getReadPointer (channel);

            for (int i = 0; i < stdPathNoSearchBuffer.getNumSamples(); ++i)
                stdPathNoSearchPeak = std::max (stdPathNoSearchPeak, std::abs (samples[i]));
        }
    }

    stdPathNoSearchEngine.release();
    stdPathEngine.release();
    abstractionDirectory.deleteRecursively();
    standardPathDirectory.deleteRecursively();

    if (stdPathNoSearchPeak <= 0.0005f)
    {
        std::cerr << "Pd declare -stdpath abstraction without project search path produced no audio; peak="
                  << stdPathNoSearchPeak << '\n';
        return 109;
    }

    std::cout << "stdPathNoSearchPeak=" << stdPathNoSearchPeak << '\n';

    const juce::String guiPatch =
        "#N canvas 120 120 420 260 10;\n"
        "#X obj 46 42 r otherware_gui_smoke;\n"
        "#X msg 46 82 0 \\, 0.22 5 \\, 0 250 5;\n"
        "#X obj 46 118 vline~;\n"
        "#X obj 154 82 osc~ 770;\n"
        "#X obj 154 118 *~;\n"
        "#X obj 154 158 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";

    ScDiscAudioEngine guiOnlyEngine;
    guiOnlyEngine.prepare (44100.0, 512, 2);

    if (! guiOnlyEngine.triggerPdGui (guiPatch, "otherware_gui_smoke", 1.0f, true, 0.25f, {}))
    {
        std::cerr << "Pd GUI receiver trigger failed\n";
        return 6;
    }

    juce::AudioBuffer<float> guiBuffer (2, 512);
    auto guiPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        guiOnlyEngine.render (guiBuffer);

        for (int channel = 0; channel < guiBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = guiBuffer.getReadPointer (channel);

            for (int i = 0; i < guiBuffer.getNumSamples(); ++i)
                guiPeak = std::max (guiPeak, std::abs (samples[i]));
        }
    }

    guiOnlyEngine.release();

    if (guiPeak <= 0.0005f)
    {
        std::cerr << "Pd GUI receiver trigger produced no audio; peak="
                  << guiPeak << '\n';
        return 7;
    }

    std::cout << "guiPeak=" << guiPeak << '\n';

    const juce::String guiFloatPatch =
        "#N canvas 120 120 480 300 10;\n"
        "#X obj 46 42 r otherware_gui_float_smoke;\n"
        "#X obj 46 78 mtof;\n"
        "#X obj 46 114 osc~;\n"
        "#X msg 164 78 0 \\, 0.20 5 \\, 0 240 5;\n"
        "#X obj 164 114 vline~;\n"
        "#X obj 46 154 *~;\n"
        "#X obj 46 194 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 3 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 5 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine guiFloatEngine;
    guiFloatEngine.prepare (44100.0, 512, 2);

    if (! guiFloatEngine.triggerPdGui (guiFloatPatch, "otherware_gui_float_smoke", 69.0f, false, 0.25f, {}))
    {
        std::cerr << "Pd GUI float receiver trigger failed\n";
        return 45;
    }

    juce::AudioBuffer<float> guiFloatBuffer (2, 512);
    auto guiFloatPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        guiFloatEngine.render (guiFloatBuffer);

        for (int channel = 0; channel < guiFloatBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = guiFloatBuffer.getReadPointer (channel);

            for (int i = 0; i < guiFloatBuffer.getNumSamples(); ++i)
                guiFloatPeak = std::max (guiFloatPeak, std::abs (samples[i]));
        }
    }

    guiFloatEngine.release();

    if (guiFloatPeak <= 0.0005f)
    {
        std::cerr << "Pd GUI float receiver trigger produced no audio; peak="
                  << guiFloatPeak << '\n';
        return 46;
    }

    std::cout << "guiFloatPeak=" << guiFloatPeak << '\n';

    const juce::String guiOutletPatch =
        "#N canvas 120 120 520 320 10;\n"
        "#X obj 46 42 bng 20 250 50 0 empty empty fire 0 -10 0 12 #fcfcfc #000000 #000000;\n"
        "#X msg 46 88 0 \\, 0.18 5 \\, 0 220 5;\n"
        "#X obj 46 128 vline~;\n"
        "#X obj 184 88 osc~ 880;\n"
        "#X obj 184 128 *~;\n"
        "#X obj 184 168 dac~;\n"
        "#X obj 260 42 r otherware_gui_outlet_smoke;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 6 0 1 0;\n";

    ScDiscAudioEngine guiOutletEngine;
    guiOutletEngine.prepare (44100.0, 512, 2);

    if (! guiOutletEngine.triggerPdMessage (guiOutletPatch,
                                            "otherware_gui_outlet_smoke",
                                            "bang",
                                            {},
                                            0.25f,
                                            {}))
    {
        std::cerr << "Pd GUI outlet fallback trigger failed\n";
        return 47;
    }

    juce::AudioBuffer<float> guiOutletBuffer (2, 512);
    auto guiOutletPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        guiOutletEngine.render (guiOutletBuffer);

        for (int channel = 0; channel < guiOutletBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = guiOutletBuffer.getReadPointer (channel);

            for (int i = 0; i < guiOutletBuffer.getNumSamples(); ++i)
                guiOutletPeak = std::max (guiOutletPeak, std::abs (samples[i]));
        }
    }

    guiOutletEngine.release();

    if (guiOutletPeak <= 0.0005f)
    {
        std::cerr << "Pd GUI outlet fallback produced no audio; peak="
                  << guiOutletPeak << '\n';
        return 48;
    }

    std::cout << "guiOutletPeak=" << guiOutletPeak << '\n';

    const juce::String semicolonMessagePatch =
        "#N canvas 120 120 540 320 10;\n"
        "#X obj 46 42 loadbang;\n"
        "#X msg 46 82 \\; otherware_semicolon_env 0 \\, 0.17 5 \\, 0 210 5;\n"
        "#X obj 258 82 r otherware_semicolon_env;\n"
        "#X obj 258 122 vline~;\n"
        "#X obj 390 82 osc~ 660;\n"
        "#X obj 390 122 *~;\n"
        "#X obj 390 162 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    DiscAudioTrigger semicolonMessageTrigger;
    semicolonMessageTrigger.depth = 1;
    semicolonMessageTrigger.branchIndex = 48;
    semicolonMessageTrigger.pdDurationSeconds = 0.25f;
    semicolonMessageTrigger.pdPatch = semicolonMessagePatch;

    ScDiscAudioEngine semicolonMessageEngine;
    semicolonMessageEngine.prepare (44100.0, 512, 2);
    semicolonMessageEngine.trigger (semicolonMessageTrigger);

    juce::AudioBuffer<float> semicolonMessageBuffer (2, 512);
    auto semicolonMessagePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        semicolonMessageEngine.render (semicolonMessageBuffer);

        for (int channel = 0; channel < semicolonMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = semicolonMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < semicolonMessageBuffer.getNumSamples(); ++i)
                semicolonMessagePeak = std::max (semicolonMessagePeak, std::abs (samples[i]));
        }
    }

    semicolonMessageEngine.release();

    if (semicolonMessagePeak <= 0.0005f)
    {
        std::cerr << "Pd semicolon message fixture produced no audio; peak="
                  << semicolonMessagePeak << '\n';
        return 49;
    }

    std::cout << "semicolonMessagePeak=" << semicolonMessagePeak << '\n';

    const juce::String multiSendMessagePatch =
        "#N canvas 120 120 620 360 10;\n"
        "#X obj 46 42 loadbang;\n"
        "#X msg 46 82 \\; otherware_multisend_env 0 \\, 0.15 5 \\, 0 210 5 \\; otherware_multisend_pitch 770;\n"
        "#X obj 258 74 r otherware_multisend_env;\n"
        "#X obj 258 114 vline~;\n"
        "#X obj 430 74 r otherware_multisend_pitch;\n"
        "#X obj 430 114 osc~;\n"
        "#X obj 430 154 *~;\n"
        "#X obj 430 194 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 6 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    DiscAudioTrigger multiSendMessageTrigger;
    multiSendMessageTrigger.depth = 1;
    multiSendMessageTrigger.branchIndex = 49;
    multiSendMessageTrigger.pdDurationSeconds = 0.25f;
    multiSendMessageTrigger.pdPatch = multiSendMessagePatch;

    ScDiscAudioEngine multiSendMessageEngine;
    multiSendMessageEngine.prepare (44100.0, 512, 2);
    multiSendMessageEngine.trigger (multiSendMessageTrigger);

    juce::AudioBuffer<float> multiSendMessageBuffer (2, 512);
    auto multiSendMessagePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        multiSendMessageEngine.render (multiSendMessageBuffer);

        for (int channel = 0; channel < multiSendMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = multiSendMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < multiSendMessageBuffer.getNumSamples(); ++i)
                multiSendMessagePeak = std::max (multiSendMessagePeak, std::abs (samples[i]));
        }
    }

    multiSendMessageEngine.release();

    if (multiSendMessagePeak <= 0.0005f)
    {
        std::cerr << "Pd multi-send message fixture produced no audio; peak="
                  << multiSendMessagePeak << '\n';
        return 50;
    }

    std::cout << "multiSendMessagePeak=" << multiSendMessagePeak << '\n';

    const juce::String dollarMessagePatch =
        "#N canvas 120 120 520 320 10;\n"
        "#X obj 46 42 r otherware_dollar_message_smoke;\n"
        "#X msg 46 82 0 \\, \\$1 5 \\, 0 210 5;\n"
        "#X obj 46 122 vline~;\n"
        "#X obj 204 82 osc~ 930;\n"
        "#X obj 204 122 *~;\n"
        "#X obj 204 162 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";

    ScDiscAudioEngine dollarMessageEngine;
    dollarMessageEngine.prepare (44100.0, 512, 2);

    if (! dollarMessageEngine.triggerPdMessage (dollarMessagePatch,
                                                "otherware_dollar_message_smoke",
                                                "float",
                                                { "0.16" },
                                                0.25f,
                                                {}))
    {
        std::cerr << "Pd dollar message trigger failed\n";
        return 51;
    }

    juce::AudioBuffer<float> dollarMessageBuffer (2, 512);
    auto dollarMessagePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        dollarMessageEngine.render (dollarMessageBuffer);

        for (int channel = 0; channel < dollarMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = dollarMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < dollarMessageBuffer.getNumSamples(); ++i)
                dollarMessagePeak = std::max (dollarMessagePeak, std::abs (samples[i]));
        }
    }

    dollarMessageEngine.release();

    if (dollarMessagePeak <= 0.0005f)
    {
        std::cerr << "Pd dollar message fixture produced no audio; peak="
                  << dollarMessagePeak << '\n';
        return 52;
    }

    std::cout << "dollarMessagePeak=" << dollarMessagePeak << '\n';

    const juce::String localDollarPatch =
        "#N canvas 120 120 540 320 10;\n"
        "#X obj 46 42 loadbang;\n"
        "#X msg 46 82 0 \\, 0.14 5 \\, 0 210 5;\n"
        "#X obj 46 122 s \\$0-env;\n"
        "#X obj 258 82 r \\$0-env;\n"
        "#X obj 258 122 vline~;\n"
        "#X obj 390 82 osc~ 520;\n"
        "#X obj 390 122 *~;\n"
        "#X obj 390 162 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 6 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    DiscAudioTrigger localDollarTrigger;
    localDollarTrigger.depth = 1;
    localDollarTrigger.branchIndex = 50;
    localDollarTrigger.pdDurationSeconds = 0.25f;
    localDollarTrigger.pdPatch = localDollarPatch;

    ScDiscAudioEngine localDollarEngine;
    localDollarEngine.prepare (44100.0, 512, 2);
    localDollarEngine.trigger (localDollarTrigger);

    juce::AudioBuffer<float> localDollarBuffer (2, 512);
    auto localDollarPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        localDollarEngine.render (localDollarBuffer);

        for (int channel = 0; channel < localDollarBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = localDollarBuffer.getReadPointer (channel);

            for (int i = 0; i < localDollarBuffer.getNumSamples(); ++i)
                localDollarPeak = std::max (localDollarPeak, std::abs (samples[i]));
        }
    }

    localDollarEngine.release();

    if (localDollarPeak <= 0.0005f)
    {
        std::cerr << "Pd $0 local send fixture produced no audio; peak="
                  << localDollarPeak << '\n';
        return 53;
    }

    std::cout << "localDollarPeak=" << localDollarPeak << '\n';

    const juce::String messagePatch =
        "#N canvas 120 120 460 280 10;\n"
        "#X obj 46 42 r otherware_message_smoke;\n"
        "#X obj 46 78 unpack f f;\n"
        "#X obj 46 114 osc~;\n"
        "#X msg 154 114 0 \\, 0.18 5 \\, 0 220 5;\n"
        "#X obj 154 150 vline~;\n"
        "#X obj 46 186 *~;\n"
        "#X obj 46 226 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 3 0;\n"
        "#X connect 2 0 5 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine messageEngine;
    messageEngine.prepare (44100.0, 512, 2);

    if (! messageEngine.triggerPdMessage (messagePatch,
                                          "otherware_message_smoke",
                                          "list",
                                          { "990", "1" },
                                          0.25f,
                                          {}))
    {
        std::cerr << "Pd list message trigger failed\n";
        return 8;
    }

    juce::AudioBuffer<float> messageBuffer (2, 512);
    auto messagePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        messageEngine.render (messageBuffer);

        for (int channel = 0; channel < messageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = messageBuffer.getReadPointer (channel);

            for (int i = 0; i < messageBuffer.getNumSamples(); ++i)
                messagePeak = std::max (messagePeak, std::abs (samples[i]));
        }
    }

    messageEngine.release();

    if (messagePeak <= 0.0005f)
    {
        std::cerr << "Pd list message trigger produced no audio; peak="
                  << messagePeak << '\n';
        return 9;
    }

    std::cout << "messagePeak=" << messagePeak << '\n';

    ScDiscAudioEngine repeatedMessageEngine;
    repeatedMessageEngine.prepare (44100.0, 128, 2);

    if (! repeatedMessageEngine.triggerPdMessage (messagePatch,
                                                 "otherware_message_smoke",
                                                 "list",
                                                 { "660", "1" },
                                                 0.02f,
                                                 {}))
    {
        std::cerr << "Pd repeated message first trigger failed\n";
        return 48;
    }

    juce::AudioBuffer<float> repeatedMessageBuffer (2, 128);
    auto repeatedMessageFirstPeak = 0.0f;

    for (int block = 0; block < 8; ++block)
    {
        repeatedMessageEngine.render (repeatedMessageBuffer);

        for (int channel = 0; channel < repeatedMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = repeatedMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < repeatedMessageBuffer.getNumSamples(); ++i)
                repeatedMessageFirstPeak = std::max (repeatedMessageFirstPeak, std::abs (samples[i]));
        }
    }

    if (! repeatedMessageEngine.triggerPdMessage (messagePatch,
                                                 "otherware_message_smoke",
                                                 "list",
                                                 { "990", "1" },
                                                 0.02f,
                                                 {}))
    {
        std::cerr << "Pd repeated message second trigger failed\n";
        return 49;
    }

    auto repeatedMessageSecondPeak = 0.0f;

    for (int block = 0; block < 8; ++block)
    {
        repeatedMessageEngine.render (repeatedMessageBuffer);

        for (int channel = 0; channel < repeatedMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = repeatedMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < repeatedMessageBuffer.getNumSamples(); ++i)
                repeatedMessageSecondPeak = std::max (repeatedMessageSecondPeak, std::abs (samples[i]));
        }
    }

    repeatedMessageEngine.release();

    if (repeatedMessageFirstPeak <= 0.0005f || repeatedMessageSecondPeak <= 0.0005f)
    {
        std::cerr << "Pd repeated message retrigger produced no audio; first="
                  << repeatedMessageFirstPeak << " second=" << repeatedMessageSecondPeak << '\n';
        return 50;
    }

    std::cout << "repeatedMessagePeaks=" << repeatedMessageFirstPeak
              << "/" << repeatedMessageSecondPeak << '\n';

    DiscAudioTrigger pdFormattedReceiverTrigger;
    pdFormattedReceiverTrigger.depth = 1;
    pdFormattedReceiverTrigger.branchIndex = 56;
    pdFormattedReceiverTrigger.pdDurationSeconds = 0.25f;
    pdFormattedReceiverTrigger.pdPatch =
        "#N canvas 120 120 460 280 10;\n"
        "#X obj 46 42 r trigger, f 18;\n"
        "#X msg 46 82 0 \\, 0.19 5 \\, 0 220 5;\n"
        "#X obj 46 118 vline~;\n"
        "#X obj 154 82 osc~ 735;\n"
        "#X obj 154 118 *~;\n"
        "#X obj 154 158 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";

    ScDiscAudioEngine formattedReceiverEngine;
    formattedReceiverEngine.prepare (44100.0, 512, 2);
    formattedReceiverEngine.trigger (pdFormattedReceiverTrigger);

    juce::AudioBuffer<float> formattedReceiverBuffer (2, 512);
    auto formattedReceiverPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        formattedReceiverEngine.render (formattedReceiverBuffer);

        for (int channel = 0; channel < formattedReceiverBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = formattedReceiverBuffer.getReadPointer (channel);

            for (int i = 0; i < formattedReceiverBuffer.getNumSamples(); ++i)
                formattedReceiverPeak = std::max (formattedReceiverPeak, std::abs (samples[i]));
        }
    }

    formattedReceiverEngine.release();

    if (formattedReceiverPeak <= 0.0005f)
    {
        std::cerr << "Pd formatted r trigger receiver produced no audio; peak="
                  << formattedReceiverPeak << '\n';
        return 51;
    }

    std::cout << "formattedReceiverPeak=" << formattedReceiverPeak << '\n';

    const juce::String clickedMessagePatch =
        "#N canvas 120 120 500 300 10;\n"
        "#X msg 46 78 720 1;\n"
        "#X obj 46 118 unpack f f;\n"
        "#X obj 46 154 osc~;\n"
        "#X msg 154 154 0 \\, 0.16 5 \\, 0 190 5;\n"
        "#X obj 154 190 vline~;\n"
        "#X obj 46 226 *~;\n"
        "#X obj 46 266 dac~;\n"
        "#X obj 46 24 r otherware_message_click_smoke;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 3 0;\n"
        "#X connect 2 0 5 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n"
        "#X connect 7 0 0 0;\n";

    ScDiscAudioEngine clickedMessageEngine;
    clickedMessageEngine.prepare (44100.0, 512, 2);

    if (! clickedMessageEngine.triggerPdMessage (clickedMessagePatch,
                                                 "otherware_message_click_smoke",
                                                 "bang",
                                                 {},
                                                 0.25f,
                                                 {}))
    {
        std::cerr << "Pd clicked message trigger failed\n";
        return 78;
    }

    juce::AudioBuffer<float> clickedMessageBuffer (2, 512);
    auto clickedMessagePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        clickedMessageEngine.render (clickedMessageBuffer);

        for (int channel = 0; channel < clickedMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = clickedMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < clickedMessageBuffer.getNumSamples(); ++i)
                clickedMessagePeak = std::max (clickedMessagePeak, std::abs (samples[i]));
        }
    }

    clickedMessageEngine.release();

    if (clickedMessagePeak <= 0.0005f)
    {
        std::cerr << "Pd clicked message fixture produced no audio; peak="
                  << clickedMessagePeak << '\n';
        return 79;
    }

    std::cout << "clickedMessagePeak=" << clickedMessagePeak << '\n';

    const juce::String clickedSemicolonMessagePatch =
        "#N canvas 120 120 560 320 10;\n"
        "#X obj 46 24 r otherware_semicolon_click_smoke;\n"
        "#X msg 46 66 \\; otherware_semiclick_env 0 \\, 0.18 5 \\, 0 220 5 \\; otherware_semiclick_pitch 660;\n"
        "#X obj 244 82 r otherware_semiclick_env;\n"
        "#X obj 244 122 vline~;\n"
        "#X obj 410 82 r otherware_semiclick_pitch;\n"
        "#X obj 410 122 osc~;\n"
        "#X obj 410 162 *~;\n"
        "#X obj 410 202 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 6 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine clickedSemicolonMessageEngine;
    clickedSemicolonMessageEngine.prepare (44100.0, 512, 2);

    if (! clickedSemicolonMessageEngine.triggerPdMessage (clickedSemicolonMessagePatch,
                                                          "otherware_semicolon_click_smoke",
                                                          "bang",
                                                          {},
                                                          0.25f,
                                                          {}))
    {
        std::cerr << "Pd clicked semicolon message trigger failed\n";
        return 80;
    }

    juce::AudioBuffer<float> clickedSemicolonMessageBuffer (2, 512);
    auto clickedSemicolonMessagePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        clickedSemicolonMessageEngine.render (clickedSemicolonMessageBuffer);

        for (int channel = 0; channel < clickedSemicolonMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = clickedSemicolonMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < clickedSemicolonMessageBuffer.getNumSamples(); ++i)
                clickedSemicolonMessagePeak = std::max (clickedSemicolonMessagePeak, std::abs (samples[i]));
        }
    }

    clickedSemicolonMessageEngine.release();

    if (clickedSemicolonMessagePeak <= 0.0005f)
    {
        std::cerr << "Pd clicked semicolon message fixture produced no audio; peak="
                  << clickedSemicolonMessagePeak << '\n';
        return 81;
    }

    std::cout << "clickedSemicolonMessagePeak=" << clickedSemicolonMessagePeak << '\n';

    const juce::String selectorPatch =
        "#N canvas 120 120 460 280 10;\n"
        "#X obj 46 42 r otherware_selector_smoke;\n"
        "#X obj 46 78 route ping;\n"
        "#X obj 46 114 osc~;\n"
        "#X msg 154 114 0 \\, 0.16 5 \\, 0 210 5;\n"
        "#X obj 154 150 vline~;\n"
        "#X obj 46 186 *~;\n"
        "#X obj 46 226 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 3 0;\n"
        "#X connect 2 0 5 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine selectorEngine;
    selectorEngine.prepare (44100.0, 512, 2);

    if (! selectorEngine.triggerPdMessage (selectorPatch,
                                           "otherware_selector_smoke",
                                           "ping",
                                           { "880" },
                                           0.25f,
                                           {}))
    {
        std::cerr << "Pd selector message trigger failed\n";
        return 10;
    }

    juce::AudioBuffer<float> selectorBuffer (2, 512);
    auto selectorPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        selectorEngine.render (selectorBuffer);

        for (int channel = 0; channel < selectorBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = selectorBuffer.getReadPointer (channel);

            for (int i = 0; i < selectorBuffer.getNumSamples(); ++i)
                selectorPeak = std::max (selectorPeak, std::abs (samples[i]));
        }
    }

    selectorEngine.release();

    if (selectorPeak <= 0.0005f)
    {
        std::cerr << "Pd selector message trigger produced no audio; peak="
                  << selectorPeak << '\n';
        return 11;
    }

    std::cout << "selectorPeak=" << selectorPeak << '\n';

    const juce::String symbolPatch =
        "#N canvas 120 120 460 280 10;\n"
        "#X obj 46 42 r otherware_symbol_smoke;\n"
        "#X obj 46 78 select ready;\n"
        "#X msg 46 114 0 \\, 0.14 5 \\, 0 190 5;\n"
        "#X obj 46 150 vline~;\n"
        "#X obj 154 114 osc~ 440;\n"
        "#X obj 154 150 *~;\n"
        "#X obj 154 190 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine symbolEngine;
    symbolEngine.prepare (44100.0, 512, 2);

    if (! symbolEngine.triggerPdMessage (symbolPatch,
                                         "otherware_symbol_smoke",
                                         "symbol",
                                         { "ready" },
                                         0.25f,
                                         {}))
    {
        std::cerr << "Pd symbol message trigger failed\n";
        return 12;
    }

    juce::AudioBuffer<float> symbolBuffer (2, 512);
    auto symbolPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        symbolEngine.render (symbolBuffer);

        for (int channel = 0; channel < symbolBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = symbolBuffer.getReadPointer (channel);

            for (int i = 0; i < symbolBuffer.getNumSamples(); ++i)
                symbolPeak = std::max (symbolPeak, std::abs (samples[i]));
        }
    }

    symbolEngine.release();

    if (symbolPeak <= 0.0005f)
    {
        std::cerr << "Pd symbol message trigger produced no audio; peak="
                  << symbolPeak << '\n';
        return 13;
    }

    std::cout << "symbolPeak=" << symbolPeak << '\n';

    const juce::String escapedSymbolPatch =
        "#N canvas 120 120 500 300 10;\n"
        "#X obj 46 42 r otherware_escaped_symbol_smoke;\n"
        "#X obj 46 78 select ready\\ now;\n"
        "#X msg 46 114 0 \\, 0.14 5 \\, 0 190 5;\n"
        "#X obj 46 150 vline~;\n"
        "#X obj 154 114 osc~ 470;\n"
        "#X obj 154 150 *~;\n"
        "#X obj 154 190 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine escapedSymbolEngine;
    escapedSymbolEngine.prepare (44100.0, 512, 2);

    if (! escapedSymbolEngine.triggerPdMessage (escapedSymbolPatch,
                                                "otherware_escaped_symbol_smoke",
                                                "symbol",
                                                { "ready\\ now" },
                                                0.25f,
                                                {}))
    {
        std::cerr << "Pd escaped symbol message trigger failed\n";
        return 110;
    }

    juce::AudioBuffer<float> escapedSymbolBuffer (2, 512);
    auto escapedSymbolPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        escapedSymbolEngine.render (escapedSymbolBuffer);

        for (int channel = 0; channel < escapedSymbolBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = escapedSymbolBuffer.getReadPointer (channel);

            for (int i = 0; i < escapedSymbolBuffer.getNumSamples(); ++i)
                escapedSymbolPeak = std::max (escapedSymbolPeak, std::abs (samples[i]));
        }
    }

    escapedSymbolEngine.release();

    if (escapedSymbolPeak <= 0.0005f)
    {
        std::cerr << "Pd escaped symbol message trigger produced no audio; peak="
                  << escapedSymbolPeak << '\n';
        return 111;
    }

    std::cout << "escapedSymbolPeak=" << escapedSymbolPeak << '\n';

    const juce::String controlFlowPatch =
        "#N canvas 120 120 560 360 10;\n"
        "#X obj 46 42 r otherware_control_flow_smoke;\n"
        "#X obj 46 82 t b f;\n"
        "#X obj 164 122 moses 1;\n"
        "#X msg 214 162 1;\n"
        "#X obj 46 202 spigot 0;\n"
        "#X msg 46 242 0 \\, 0.18 5 \\, 0 220 5;\n"
        "#X obj 46 282 vline~;\n"
        "#X obj 250 242 osc~ 610;\n"
        "#X obj 250 282 *~;\n"
        "#X obj 250 322 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 4 0;\n"
        "#X connect 1 1 2 0;\n"
        "#X connect 2 1 3 0;\n"
        "#X connect 3 0 4 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 8 1;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine controlFlowEngine;
    controlFlowEngine.prepare (44100.0, 512, 2);

    if (! controlFlowEngine.triggerPdMessage (controlFlowPatch,
                                              "otherware_control_flow_smoke",
                                              "float",
                                              { "2" },
                                              0.25f,
                                              {}))
    {
        std::cerr << "Pd control-flow trigger failed\n";
        return 54;
    }

    juce::AudioBuffer<float> controlFlowBuffer (2, 512);
    auto controlFlowPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        controlFlowEngine.render (controlFlowBuffer);

        for (int channel = 0; channel < controlFlowBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = controlFlowBuffer.getReadPointer (channel);

            for (int i = 0; i < controlFlowBuffer.getNumSamples(); ++i)
                controlFlowPeak = std::max (controlFlowPeak, std::abs (samples[i]));
        }
    }

    controlFlowEngine.release();

    if (controlFlowPeak <= 0.0005f)
    {
        std::cerr << "Pd trigger/moses/spigot fixture produced no audio; peak="
                  << controlFlowPeak << '\n';
        return 55;
    }

    std::cout << "controlFlowPeak=" << controlFlowPeak << '\n';

    DiscAudioTrigger pdCompatibilityTrigger;
    pdCompatibilityTrigger.depth = 1;
    pdCompatibilityTrigger.branchIndex = 9;
    pdCompatibilityTrigger.pdDurationSeconds = 0.35f;
    pdCompatibilityTrigger.pdPatch =
        "#N canvas 80 80 720 460 10;\n"
        "#X text 42 22 compatibility: comments \\, escaped commas and GUI atoms;\n"
        "#X obj 48 58 r trigger;\n"
        "#X msg 48 98 0 \\, 0.19 5 \\, 0 260 5;\n"
        "#X obj 48 138 vline~;\n"
        "#X obj 178 98 osc~ 515;\n"
        "#X obj 178 138 *~;\n"
        "#X obj 178 178 dac~;\n"
        "#X floatatom 320 98 5 0 0 0 - - - 0;\n"
        "#X symbolatom 320 138 10 0 0 0 - - - 0;\n"
        "#X obj 320 178 bng 20 250 50 0 empty empty fire 0 -10 0 12 #fcfcfc #000000 #000000;\n"
        "#N canvas 220 220 260 180 nested 0;\n"
        "#X text 24 24 nested canvas fixture;\n"
        "#X obj 28 62 inlet;\n"
        "#X obj 28 102 outlet;\n"
        "#X connect 1 0 2 0;\n"
        "#X restore 320 218 pd nested;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine compatibilityEngine;
    compatibilityEngine.prepare (44100.0, 512, 2);
    compatibilityEngine.trigger (pdCompatibilityTrigger);

    juce::AudioBuffer<float> compatibilityBuffer (2, 512);
    auto compatibilityPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        compatibilityEngine.render (compatibilityBuffer);

        for (int channel = 0; channel < compatibilityBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = compatibilityBuffer.getReadPointer (channel);

            for (int i = 0; i < compatibilityBuffer.getNumSamples(); ++i)
                compatibilityPeak = std::max (compatibilityPeak, std::abs (samples[i]));
        }
    }

    compatibilityEngine.release();

    if (compatibilityPeak <= 0.0005f)
    {
        std::cerr << "Pd compatibility fixture produced no audio; peak="
                  << compatibilityPeak << '\n';
        return 14;
    }

    std::cout << "compatibilityPeak=" << compatibilityPeak << '\n';

    DiscAudioTrigger pdCommentIndexTrigger;
    pdCommentIndexTrigger.depth = 1;
    pdCommentIndexTrigger.branchIndex = 10;
    pdCommentIndexTrigger.pdDurationSeconds = 0.25f;
    pdCommentIndexTrigger.pdPatch =
        "#N canvas 120 120 520 320 10;\n"
        "#X text 40 28 this comment deliberately shifts Pd indices;\n"
        "#X obj 48 68 r trigger;\n"
        "#X msg 48 108 0 \\, 0.17 5 \\, 0 200 5;\n"
        "#X obj 48 148 vline~;\n"
        "#X obj 184 108 osc~ 705;\n"
        "#X obj 184 148 *~;\n"
        "#X obj 184 188 dac~;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine commentIndexEngine;
    commentIndexEngine.prepare (44100.0, 512, 2);
    commentIndexEngine.trigger (pdCommentIndexTrigger);

    juce::AudioBuffer<float> commentIndexBuffer (2, 512);
    auto commentIndexPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        commentIndexEngine.render (commentIndexBuffer);

        for (int channel = 0; channel < commentIndexBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = commentIndexBuffer.getReadPointer (channel);

            for (int i = 0; i < commentIndexBuffer.getNumSamples(); ++i)
                commentIndexPeak = std::max (commentIndexPeak, std::abs (samples[i]));
        }
    }

    commentIndexEngine.release();

    if (commentIndexPeak <= 0.0005f)
    {
        std::cerr << "Pd comment-index fixture produced no audio; peak="
                  << commentIndexPeak << '\n';
        return 15;
    }

    std::cout << "commentIndexPeak=" << commentIndexPeak << '\n';

    DiscAudioTrigger pdLoadbangTrigger;
    pdLoadbangTrigger.depth = 1;
    pdLoadbangTrigger.branchIndex = 11;
    pdLoadbangTrigger.pdDurationSeconds = 0.25f;
    pdLoadbangTrigger.pdPatch =
        "#N canvas 120 120 460 280 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 0 \\, 0.15 5 \\, 0 180 5;\n"
        "#X obj 48 118 vline~;\n"
        "#X obj 164 82 osc~ 615;\n"
        "#X obj 164 118 *~;\n"
        "#X obj 164 158 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";

    ScDiscAudioEngine loadbangEngine;
    loadbangEngine.prepare (44100.0, 512, 2);
    loadbangEngine.trigger (pdLoadbangTrigger);

    juce::AudioBuffer<float> loadbangBuffer (2, 512);
    auto loadbangPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        loadbangEngine.render (loadbangBuffer);

        for (int channel = 0; channel < loadbangBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = loadbangBuffer.getReadPointer (channel);

            for (int i = 0; i < loadbangBuffer.getNumSamples(); ++i)
                loadbangPeak = std::max (loadbangPeak, std::abs (samples[i]));
        }
    }

    loadbangEngine.release();

    if (loadbangPeak <= 0.0005f)
    {
        std::cerr << "Pd loadbang fixture produced no audio; peak="
                  << loadbangPeak << '\n';
        return 16;
    }

    std::cout << "loadbangPeak=" << loadbangPeak << '\n';

    DiscAudioTrigger pdDelayTrigger;
    pdDelayTrigger.depth = 1;
    pdDelayTrigger.branchIndex = 12;
    pdDelayTrigger.pdDurationSeconds = 0.35f;
    pdDelayTrigger.pdPatch =
        "#N canvas 120 120 500 300 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 delay 35;\n"
        "#X msg 48 122 0 \\, 0.16 5 \\, 0 210 5;\n"
        "#X obj 48 158 vline~;\n"
        "#X obj 184 122 osc~ 735;\n"
        "#X obj 184 158 *~;\n"
        "#X obj 184 198 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine delayEngine;
    delayEngine.prepare (44100.0, 512, 2);
    delayEngine.trigger (pdDelayTrigger);

    juce::AudioBuffer<float> delayBuffer (2, 512);
    auto delayPeak = 0.0f;

    for (int block = 0; block < 120; ++block)
    {
        delayEngine.render (delayBuffer);

        for (int channel = 0; channel < delayBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = delayBuffer.getReadPointer (channel);

            for (int i = 0; i < delayBuffer.getNumSamples(); ++i)
                delayPeak = std::max (delayPeak, std::abs (samples[i]));
        }
    }

    delayEngine.release();

    if (delayPeak <= 0.0005f)
    {
        std::cerr << "Pd delay fixture produced no audio; peak="
                  << delayPeak << '\n';
        return 17;
    }

    std::cout << "delayPeak=" << delayPeak << '\n';

    DiscAudioTrigger pdMetroPipeTrigger;
    pdMetroPipeTrigger.depth = 1;
    pdMetroPipeTrigger.branchIndex = 14;
    pdMetroPipeTrigger.pdDurationSeconds = 0.3f;
    pdMetroPipeTrigger.pdPatch =
        "#N canvas 120 120 560 360 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 1;\n"
        "#X obj 48 122 metro 35;\n"
        "#X msg 48 162 0.13;\n"
        "#X obj 48 202 pipe 12;\n"
        "#X msg 48 242 0 \\, \\$1 3 \\, 0 28 3;\n"
        "#X obj 48 282 vline~;\n"
        "#X obj 220 242 osc~ 745;\n"
        "#X obj 220 282 *~;\n"
        "#X obj 220 322 dac~;\n"
        "#X obj 150 82 del 150;\n"
        "#X msg 150 122 0;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 10 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 8 1;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 2 0;\n";

    ScDiscAudioEngine metroPipeEngine;
    metroPipeEngine.prepare (44100.0, 512, 2);
    metroPipeEngine.trigger (pdMetroPipeTrigger);

    juce::AudioBuffer<float> metroPipeBuffer (2, 512);
    auto metroPipePeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        metroPipeEngine.render (metroPipeBuffer);

        for (int channel = 0; channel < metroPipeBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = metroPipeBuffer.getReadPointer (channel);

            for (int i = 0; i < metroPipeBuffer.getNumSamples(); ++i)
                metroPipePeak = std::max (metroPipePeak, std::abs (samples[i]));
        }
    }

    metroPipeEngine.release();

    if (metroPipePeak <= 0.0005f)
    {
        std::cerr << "Pd metro/pipe fixture produced no audio; peak="
                  << metroPipePeak << '\n';
        return 56;
    }

    std::cout << "metroPipePeak=" << metroPipePeak << '\n';

    DiscAudioTrigger pdSendReceiveTrigger;
    pdSendReceiveTrigger.depth = 1;
    pdSendReceiveTrigger.branchIndex = 14;
    pdSendReceiveTrigger.pdDurationSeconds = 0.3f;
    pdSendReceiveTrigger.pdPatch =
        "#N canvas 120 120 540 320 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.18 5 \\, 0 220 5;\n"
        "#X obj 48 122 s otherware_env_bus;\n"
        "#X obj 208 42 r otherware_env_bus;\n"
        "#X obj 208 82 vline~;\n"
        "#X obj 344 82 osc~ 925;\n"
        "#X obj 344 122 *~;\n"
        "#X obj 344 162 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 6 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine sendReceiveEngine;
    sendReceiveEngine.prepare (44100.0, 512, 2);
    sendReceiveEngine.trigger (pdSendReceiveTrigger);

    juce::AudioBuffer<float> sendReceiveBuffer (2, 512);
    auto sendReceivePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        sendReceiveEngine.render (sendReceiveBuffer);

        for (int channel = 0; channel < sendReceiveBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = sendReceiveBuffer.getReadPointer (channel);

            for (int i = 0; i < sendReceiveBuffer.getNumSamples(); ++i)
                sendReceivePeak = std::max (sendReceivePeak, std::abs (samples[i]));
        }
    }

    sendReceiveEngine.release();

    if (sendReceivePeak <= 0.0005f)
    {
        std::cerr << "Pd send/receive fixture produced no audio; peak="
                  << sendReceivePeak << '\n';
        return 19;
    }

    std::cout << "sendReceivePeak=" << sendReceivePeak << '\n';

    DiscAudioTrigger pdSignalBusTrigger;
    pdSignalBusTrigger.depth = 1;
    pdSignalBusTrigger.branchIndex = 15;
    pdSignalBusTrigger.pdDurationSeconds = 0.3f;
    pdSignalBusTrigger.pdPatch =
        "#N canvas 120 120 560 340 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.16 5 \\, 0 220 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 980;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 184 162 send~ otherware_signal_bus;\n"
        "#X obj 374 122 receive~ otherware_signal_bus;\n"
        "#X obj 374 162 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine signalBusEngine;
    signalBusEngine.prepare (44100.0, 512, 2);
    signalBusEngine.trigger (pdSignalBusTrigger);

    juce::AudioBuffer<float> signalBusBuffer (2, 512);
    auto signalBusPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        signalBusEngine.render (signalBusBuffer);

        for (int channel = 0; channel < signalBusBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = signalBusBuffer.getReadPointer (channel);

            for (int i = 0; i < signalBusBuffer.getNumSamples(); ++i)
                signalBusPeak = std::max (signalBusPeak, std::abs (samples[i]));
        }
    }

    signalBusEngine.release();

    if (signalBusPeak <= 0.0005f)
    {
        std::cerr << "Pd send~/receive~ fixture produced no audio; peak="
                  << signalBusPeak << '\n';
        return 20;
    }

    std::cout << "signalBusPeak=" << signalBusPeak << '\n';

    DiscAudioTrigger pdDelayLineTrigger;
    pdDelayLineTrigger.depth = 1;
    pdDelayLineTrigger.branchIndex = 16;
    pdDelayLineTrigger.pdDurationSeconds = 0.35f;
    pdDelayLineTrigger.pdPatch =
        "#N canvas 120 120 620 380 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.2 5 \\, 0 90 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 720;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 184 162 delwrite~ otherware_delay_line 160;\n"
        "#X obj 384 122 delread~ otherware_delay_line 45;\n"
        "#X obj 384 162 *~ 0.8;\n"
        "#X obj 384 202 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 7 0 8 1;\n";

    ScDiscAudioEngine delayLineEngine;
    delayLineEngine.prepare (44100.0, 512, 2);
    delayLineEngine.trigger (pdDelayLineTrigger);

    juce::AudioBuffer<float> delayLineBuffer (2, 512);
    auto delayLinePeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        delayLineEngine.render (delayLineBuffer);

        for (int channel = 0; channel < delayLineBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = delayLineBuffer.getReadPointer (channel);

            for (int i = 0; i < delayLineBuffer.getNumSamples(); ++i)
                delayLinePeak = std::max (delayLinePeak, std::abs (samples[i]));
        }
    }

    delayLineEngine.release();

    if (delayLinePeak <= 0.0005f)
    {
        std::cerr << "Pd delwrite~/delread~ fixture produced no audio; peak="
                  << delayLinePeak << '\n';
        return 57;
    }

    std::cout << "delayLinePeak=" << delayLinePeak << '\n';

    DiscAudioTrigger pdInterpolatedDelayTrigger;
    pdInterpolatedDelayTrigger.depth = 1;
    pdInterpolatedDelayTrigger.branchIndex = 17;
    pdInterpolatedDelayTrigger.pdDurationSeconds = 0.35f;
    pdInterpolatedDelayTrigger.pdPatch =
        "#N canvas 120 120 660 400 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.18 5 \\, 0 90 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 530;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 184 162 delwrite~ otherware_interp_delay 180;\n"
        "#X obj 384 82 sig~ 55;\n"
        "#X obj 384 122 delread4~ otherware_interp_delay;\n"
        "#X obj 384 162 *~ 0.75;\n"
        "#X obj 384 202 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine interpolatedDelayEngine;
    interpolatedDelayEngine.prepare (44100.0, 512, 2);
    interpolatedDelayEngine.trigger (pdInterpolatedDelayTrigger);

    juce::AudioBuffer<float> interpolatedDelayBuffer (2, 512);
    auto interpolatedDelayPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        interpolatedDelayEngine.render (interpolatedDelayBuffer);

        for (int channel = 0; channel < interpolatedDelayBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = interpolatedDelayBuffer.getReadPointer (channel);

            for (int i = 0; i < interpolatedDelayBuffer.getNumSamples(); ++i)
                interpolatedDelayPeak = std::max (interpolatedDelayPeak, std::abs (samples[i]));
        }
    }

    interpolatedDelayEngine.release();

    if (interpolatedDelayPeak <= 0.0005f)
    {
        std::cerr << "Pd delread4~ fixture produced no audio; peak="
                  << interpolatedDelayPeak << '\n';
        return 58;
    }

    std::cout << "interpolatedDelayPeak=" << interpolatedDelayPeak << '\n';

    DiscAudioTrigger pdVariableDelayTrigger;
    pdVariableDelayTrigger.depth = 1;
    pdVariableDelayTrigger.branchIndex = 18;
    pdVariableDelayTrigger.pdDurationSeconds = 0.35f;
    pdVariableDelayTrigger.pdPatch =
        "#N canvas 120 120 660 400 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.17 5 \\, 0 90 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 470;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 184 162 delwrite~ otherware_variable_delay 180;\n"
        "#X obj 384 82 sig~ 65;\n"
        "#X obj 384 122 vd~ otherware_variable_delay;\n"
        "#X obj 384 162 *~ 0.75;\n"
        "#X obj 384 202 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine variableDelayEngine;
    variableDelayEngine.prepare (44100.0, 512, 2);
    variableDelayEngine.trigger (pdVariableDelayTrigger);

    juce::AudioBuffer<float> variableDelayBuffer (2, 512);
    auto variableDelayPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        variableDelayEngine.render (variableDelayBuffer);

        for (int channel = 0; channel < variableDelayBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = variableDelayBuffer.getReadPointer (channel);

            for (int i = 0; i < variableDelayBuffer.getNumSamples(); ++i)
                variableDelayPeak = std::max (variableDelayPeak, std::abs (samples[i]));
        }
    }

    variableDelayEngine.release();

    if (variableDelayPeak <= 0.0005f)
    {
        std::cerr << "Pd vd~ fixture produced no audio; peak="
                  << variableDelayPeak << '\n';
        return 59;
    }

    std::cout << "variableDelayPeak=" << variableDelayPeak << '\n';

    DiscAudioTrigger pdFilterChainTrigger;
    pdFilterChainTrigger.depth = 1;
    pdFilterChainTrigger.branchIndex = 19;
    pdFilterChainTrigger.pdDurationSeconds = 0.3f;
    pdFilterChainTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.22 5 \\, 0 220 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 640;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 184 162 lop~ 1800;\n"
        "#X obj 324 162 hip~ 120;\n"
        "#X obj 464 162 bp~ 640 4;\n"
        "#X obj 604 122 sig~ 640;\n"
        "#X obj 604 162 vcf~ 2;\n"
        "#X obj 324 242 *~ 0.22;\n"
        "#X obj 324 282 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 6 0;\n"
        "#X connect 4 0 7 0;\n"
        "#X connect 4 0 9 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 10 0;\n"
        "#X connect 7 0 10 0;\n"
        "#X connect 8 0 9 1;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 11 0;\n"
        "#X connect 10 0 11 1;\n";

    ScDiscAudioEngine filterChainEngine;
    filterChainEngine.prepare (44100.0, 512, 2);
    filterChainEngine.trigger (pdFilterChainTrigger);

    juce::AudioBuffer<float> filterChainBuffer (2, 512);
    auto filterChainPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        filterChainEngine.render (filterChainBuffer);

        for (int channel = 0; channel < filterChainBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = filterChainBuffer.getReadPointer (channel);

            for (int i = 0; i < filterChainBuffer.getNumSamples(); ++i)
                filterChainPeak = std::max (filterChainPeak, std::abs (samples[i]));
        }
    }

    filterChainEngine.release();

    if (filterChainPeak <= 0.0005f)
    {
        std::cerr << "Pd lop~/hip~/bp~/vcf~ fixture produced no audio; peak="
                  << filterChainPeak << '\n';
        return 60;
    }

    std::cout << "filterChainPeak=" << filterChainPeak << '\n';

    DiscAudioTrigger pdSlopTrigger;
    pdSlopTrigger.depth = 1;
    pdSlopTrigger.branchIndex = 38;
    pdSlopTrigger.pdDurationSeconds = 0.3f;
    pdSlopTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.25 5 \\, 0 220 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 410;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 324 82 sig~ 1200;\n"
        "#X obj 324 122 sig~ 0.2;\n"
        "#X obj 324 162 sig~ 4000;\n"
        "#X obj 324 202 sig~ 0.2;\n"
        "#X obj 324 242 sig~ 4000;\n"
        "#X obj 184 202 slop~ 1200 0.2 4000 0.2 4000;\n"
        "#X obj 184 242 *~ 0.24;\n"
        "#X obj 184 282 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 10 0;\n"
        "#X connect 5 0 10 1;\n"
        "#X connect 6 0 10 2;\n"
        "#X connect 7 0 10 3;\n"
        "#X connect 8 0 10 4;\n"
        "#X connect 9 0 10 5;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 11 0 12 1;\n";

    ScDiscAudioEngine slopEngine;
    slopEngine.prepare (44100.0, 512, 2);
    slopEngine.trigger (pdSlopTrigger);

    juce::AudioBuffer<float> slopBuffer (2, 512);
    auto slopPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        slopEngine.render (slopBuffer);

        for (int channel = 0; channel < slopBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = slopBuffer.getReadPointer (channel);

            for (int i = 0; i < slopBuffer.getNumSamples(); ++i)
                slopPeak = std::max (slopPeak, std::abs (samples[i]));
        }
    }

    slopEngine.release();

    if (slopPeak <= 0.0005f)
    {
        std::cerr << "Pd slop~ fixture produced no audio; peak="
                  << slopPeak << '\n';
        return 61;
    }

    std::cout << "slopPeak=" << slopPeak << '\n';

    DiscAudioTrigger pdPoleZeroTrigger;
    pdPoleZeroTrigger.depth = 1;
    pdPoleZeroTrigger.branchIndex = 20;
    pdPoleZeroTrigger.pdDurationSeconds = 0.3f;
    pdPoleZeroTrigger.pdPatch =
        "#N canvas 120 120 760 440 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.2 5 \\, 0 220 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 phasor~ 280;\n"
        "#X obj 184 122 -~ 0.5;\n"
        "#X obj 184 162 *~;\n"
        "#X obj 184 202 biquad~ 0.2 0.1 0.5 0.2 0.1;\n"
        "#X obj 184 242 rpole~ 0.62;\n"
        "#X obj 184 282 rzero~ -0.35;\n"
        "#X obj 184 322 *~ 0.38;\n"
        "#X obj 184 362 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 5 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 10 1;\n";

    ScDiscAudioEngine poleZeroEngine;
    poleZeroEngine.prepare (44100.0, 512, 2);
    poleZeroEngine.trigger (pdPoleZeroTrigger);

    juce::AudioBuffer<float> poleZeroBuffer (2, 512);
    auto poleZeroPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        poleZeroEngine.render (poleZeroBuffer);

        for (int channel = 0; channel < poleZeroBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = poleZeroBuffer.getReadPointer (channel);

            for (int i = 0; i < poleZeroBuffer.getNumSamples(); ++i)
                poleZeroPeak = std::max (poleZeroPeak, std::abs (samples[i]));
        }
    }

    poleZeroEngine.release();

    if (poleZeroPeak <= 0.0005f)
    {
        std::cerr << "Pd biquad~/rpole~/rzero~ fixture produced no audio; peak="
                  << poleZeroPeak << '\n';
        return 61;
    }

    std::cout << "poleZeroPeak=" << poleZeroPeak << '\n';

    DiscAudioTrigger pdSignalAnalysisTrigger;
    pdSignalAnalysisTrigger.depth = 1;
    pdSignalAnalysisTrigger.branchIndex = 21;
    pdSignalAnalysisTrigger.pdDurationSeconds = 0.35f;
    pdSignalAnalysisTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 48 122 0 \\, 0.2 5 \\, 0 160 5;\n"
        "#X obj 48 162 vline~;\n"
        "#X obj 184 122 osc~ 710;\n"
        "#X obj 184 162 *~;\n"
        "#X obj 184 202 env~ 512;\n"
        "#X obj 184 242 moses 20;\n"
        "#X msg 246 282 1;\n"
        "#X obj 392 122 del 90;\n"
        "#X obj 392 162 snapshot~;\n"
        "#X obj 504 122 sig~ 0.16;\n"
        "#X obj 392 202 spigot 0;\n"
        "#X msg 392 242 0 \\, \\$1 5 \\, 0 200 5;\n"
        "#X obj 392 282 vline~;\n"
        "#X obj 548 242 osc~ 390;\n"
        "#X obj 548 282 *~;\n"
        "#X obj 548 322 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 9 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 1 8 0;\n"
        "#X connect 8 0 12 1;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 12 0;\n"
        "#X connect 11 0 10 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 16 1;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 16 0 17 1;\n";

    ScDiscAudioEngine signalAnalysisEngine;
    signalAnalysisEngine.prepare (44100.0, 512, 2);
    signalAnalysisEngine.trigger (pdSignalAnalysisTrigger);

    juce::AudioBuffer<float> signalAnalysisBuffer (2, 512);
    auto signalAnalysisPeak = 0.0f;

    for (int block = 0; block < 100; ++block)
    {
        signalAnalysisEngine.render (signalAnalysisBuffer);

        for (int channel = 0; channel < signalAnalysisBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = signalAnalysisBuffer.getReadPointer (channel);

            for (int i = 0; i < signalAnalysisBuffer.getNumSamples(); ++i)
                signalAnalysisPeak = std::max (signalAnalysisPeak, std::abs (samples[i]));
        }
    }

    signalAnalysisEngine.release();

    if (signalAnalysisPeak <= 0.0005f)
    {
        std::cerr << "Pd env~/snapshot~ fixture produced no audio; peak="
                  << signalAnalysisPeak << '\n';
        return 62;
    }

    std::cout << "signalAnalysisPeak=" << signalAnalysisPeak << '\n';

    DiscAudioTrigger pdSignalEventTrigger;
    pdSignalEventTrigger.depth = 1;
    pdSignalEventTrigger.branchIndex = 22;
    pdSignalEventTrigger.pdDurationSeconds = 0.3f;
    pdSignalEventTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 1 8;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 48 162 threshold~ 0.5 0 0.1 0;\n"
        "#X msg 48 202 0 \\, 0.16 5 \\, 0 190 5;\n"
        "#X obj 48 242 vline~;\n"
        "#X obj 220 202 osc~ 820;\n"
        "#X obj 220 242 *~;\n"
        "#X obj 220 282 dac~;\n"
        "#X obj 420 42 bang~;\n"
        "#X obj 420 82 snapshot~;\n"
        "#X obj 420 122 moses 0.5;\n"
        "#X msg 482 162 1;\n"
        "#X obj 482 202 spigot 0;\n"
        "#X msg 482 242 0 \\, 0.08 5 \\, 0 160 5;\n"
        "#X obj 482 282 vline~;\n"
        "#X obj 620 242 osc~ 510;\n"
        "#X obj 620 282 *~;\n"
        "#X obj 620 322 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 13 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 2 0 10 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 7 1;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 7 0 8 1;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 1 12 0;\n"
        "#X connect 12 0 13 1;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 17 1;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 16 0 18 0;\n"
        "#X connect 17 0 18 1;\n";

    ScDiscAudioEngine signalEventEngine;
    signalEventEngine.prepare (44100.0, 512, 2);
    signalEventEngine.trigger (pdSignalEventTrigger);

    juce::AudioBuffer<float> signalEventBuffer (2, 512);
    auto signalEventPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        signalEventEngine.render (signalEventBuffer);

        for (int channel = 0; channel < signalEventBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = signalEventBuffer.getReadPointer (channel);

            for (int i = 0; i < signalEventBuffer.getNumSamples(); ++i)
                signalEventPeak = std::max (signalEventPeak, std::abs (samples[i]));
        }
    }

    signalEventEngine.release();

    if (signalEventPeak <= 0.0005f)
    {
        std::cerr << "Pd threshold~/bang~ fixture produced no audio; peak="
                  << signalEventPeak << '\n';
        return 63;
    }

    std::cout << "signalEventPeak=" << signalEventPeak << '\n';

    DiscAudioTrigger pdSampleHoldTrigger;
    pdSampleHoldTrigger.depth = 1;
    pdSampleHoldTrigger.branchIndex = 23;
    pdSampleHoldTrigger.pdDurationSeconds = 0.3f;
    pdSampleHoldTrigger.pdPatch =
        "#N canvas 120 120 620 380 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.18 5 \\, 0 220 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 204 82 noise~;\n"
        "#X obj 344 82 phasor~ 95;\n"
        "#X obj 204 122 samphold~;\n"
        "#X obj 204 162 lop~ 1800;\n"
        "#X obj 204 202 *~;\n"
        "#X obj 204 242 *~ 0.55;\n"
        "#X obj 204 282 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 7 1;\n"
        "#X connect 3 0 5 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine sampleHoldEngine;
    sampleHoldEngine.prepare (44100.0, 512, 2);
    sampleHoldEngine.trigger (pdSampleHoldTrigger);

    juce::AudioBuffer<float> sampleHoldBuffer (2, 512);
    auto sampleHoldPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        sampleHoldEngine.render (sampleHoldBuffer);

        for (int channel = 0; channel < sampleHoldBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = sampleHoldBuffer.getReadPointer (channel);

            for (int i = 0; i < sampleHoldBuffer.getNumSamples(); ++i)
                sampleHoldPeak = std::max (sampleHoldPeak, std::abs (samples[i]));
        }
    }

    sampleHoldEngine.release();

    if (sampleHoldPeak <= 0.0005f)
    {
        std::cerr << "Pd noise~/samphold~ fixture produced no audio; peak="
                  << sampleHoldPeak << '\n';
        return 64;
    }

    std::cout << "sampleHoldPeak=" << sampleHoldPeak << '\n';

    DiscAudioTrigger pdLineSignalTrigger;
    pdLineSignalTrigger.depth = 1;
    pdLineSignalTrigger.branchIndex = 24;
    pdLineSignalTrigger.pdDurationSeconds = 0.3f;
    pdLineSignalTrigger.pdPatch =
        "#N canvas 120 120 520 320 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b b;\n"
        "#X msg 48 122 0;\n"
        "#X obj 126 122 del 5;\n"
        "#X msg 126 162 0.2 5;\n"
        "#X obj 230 122 del 45;\n"
        "#X msg 230 162 0 180;\n"
        "#X obj 48 212 line~;\n"
        "#X obj 300 162 osc~ 570;\n"
        "#X obj 300 212 *~;\n"
        "#X obj 300 252 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 3 0;\n"
        "#X connect 1 2 5 0;\n"
        "#X connect 2 0 7 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 7 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 9 1;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 10 1;\n";

    ScDiscAudioEngine lineSignalEngine;
    lineSignalEngine.prepare (44100.0, 512, 2);
    lineSignalEngine.trigger (pdLineSignalTrigger);

    juce::AudioBuffer<float> lineSignalBuffer (2, 512);
    auto lineSignalPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        lineSignalEngine.render (lineSignalBuffer);

        for (int channel = 0; channel < lineSignalBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = lineSignalBuffer.getReadPointer (channel);

            for (int i = 0; i < lineSignalBuffer.getNumSamples(); ++i)
                lineSignalPeak = std::max (lineSignalPeak, std::abs (samples[i]));
        }
    }

    lineSignalEngine.release();

    if (lineSignalPeak <= 0.0005f)
    {
        std::cerr << "Pd line~ fixture produced no audio; peak="
                  << lineSignalPeak << '\n';
        return 65;
    }

    std::cout << "lineSignalPeak=" << lineSignalPeak << '\n';

    const juce::String controlTimingPatch =
        "#N canvas 120 120 660 420 10;\n"
        "#X obj 48 42 r otherware_control_timing_smoke;\n"
        "#X obj 48 82 t b b b b;\n"
        "#X msg 48 122 0.17;\n"
        "#X obj 48 162 value otherware_control_timing_amp;\n"
        "#X obj 190 122 timer;\n"
        "#X obj 310 122 realtime;\n"
        "#X obj 430 122 del 20;\n"
        "#X obj 366 202 value otherware_control_timing_amp;\n"
        "#X msg 366 242 0 \\, \\$1 5 \\, 0 190 5;\n"
        "#X obj 366 282 vline~;\n"
        "#X obj 518 242 osc~ 685;\n"
        "#X obj 518 282 *~;\n"
        "#X obj 518 322 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 4 0;\n"
        "#X connect 1 2 5 0;\n"
        "#X connect 1 3 6 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 6 0 4 1;\n"
        "#X connect 6 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 11 1;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 11 0 12 1;\n"
        "#X connect 7 0 8 0;\n";

    ScDiscAudioEngine controlTimingEngine;
    controlTimingEngine.prepare (44100.0, 512, 2);

    if (! controlTimingEngine.triggerPdMessage (controlTimingPatch,
                                                "otherware_control_timing_smoke",
                                                "bang",
                                                {},
                                                0.3f,
                                                {}))
    {
        std::cerr << "Pd value/timer/realtime trigger failed\n";
        return 66;
    }

    juce::AudioBuffer<float> controlTimingBuffer (2, 512);
    auto controlTimingPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        controlTimingEngine.render (controlTimingBuffer);

        for (int channel = 0; channel < controlTimingBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = controlTimingBuffer.getReadPointer (channel);

            for (int i = 0; i < controlTimingBuffer.getNumSamples(); ++i)
                controlTimingPeak = std::max (controlTimingPeak, std::abs (samples[i]));
        }
    }

    controlTimingEngine.release();

    if (controlTimingPeak <= 0.0005f)
    {
        std::cerr << "Pd value/timer/realtime fixture produced no audio; peak="
                  << controlTimingPeak << '\n';
        return 67;
    }

    std::cout << "controlTimingPeak=" << controlTimingPeak << '\n';

    const juce::String controlUtilityPatch =
        "#N canvas 120 120 700 420 10;\n"
        "#X obj 48 42 r otherware_control_utility_smoke;\n"
        "#X obj 48 82 t b b;\n"
        "#X obj 48 122 random 1;\n"
        "#X obj 48 162 change -1;\n"
        "#X obj 48 202 makefilename tone-%d;\n"
        "#X obj 48 242 select tone-0;\n"
        "#X obj 242 122 random 1;\n"
        "#X obj 242 162 swap 99;\n"
        "#X obj 242 202 select 99;\n"
        "#X obj 48 282 spigot 0;\n"
        "#X msg 48 322 0 \\, 0.16 5 \\, 0 190 5;\n"
        "#X obj 48 362 vline~;\n"
        "#X obj 220 322 osc~ 735;\n"
        "#X obj 220 362 *~;\n"
        "#X obj 220 402 dac~;\n"
        "#X msg 242 242 1;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 6 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 9 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 15 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 13 1;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 13 0 14 1;\n"
        "#X connect 15 0 9 1;\n";

    ScDiscAudioEngine controlUtilityEngine;
    controlUtilityEngine.prepare (44100.0, 512, 2);

    if (! controlUtilityEngine.triggerPdMessage (controlUtilityPatch,
                                                 "otherware_control_utility_smoke",
                                                 "bang",
                                                 {},
                                                 0.3f,
                                                 {}))
    {
        std::cerr << "Pd random/swap/change/makefilename trigger failed\n";
        return 68;
    }

    juce::AudioBuffer<float> controlUtilityBuffer (2, 512);
    auto controlUtilityPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        controlUtilityEngine.render (controlUtilityBuffer);

        for (int channel = 0; channel < controlUtilityBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = controlUtilityBuffer.getReadPointer (channel);

            for (int i = 0; i < controlUtilityBuffer.getNumSamples(); ++i)
                controlUtilityPeak = std::max (controlUtilityPeak, std::abs (samples[i]));
        }
    }

    controlUtilityEngine.release();

    if (controlUtilityPeak <= 0.0005f)
    {
        std::cerr << "Pd random/swap/change/makefilename fixture produced no audio; peak="
                  << controlUtilityPeak << '\n';
        return 69;
    }

    std::cout << "controlUtilityPeak=" << controlUtilityPeak << '\n';

    const juce::String untilLoopPatch =
        "#N canvas 120 120 640 420 10;\n"
        "#X obj 48 42 r otherware_until_loop_smoke;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 48 122 4;\n"
        "#X obj 48 162 until;\n"
        "#X msg 182 122 0;\n"
        "#X obj 48 202 f 0;\n"
        "#X obj 48 242 t f f;\n"
        "#X obj 182 242 + 1;\n"
        "#X obj 48 282 select 3;\n"
        "#X msg 48 322 0 \\, 0.17 5 \\, 0 200 5;\n"
        "#X obj 48 362 vline~;\n"
        "#X obj 220 322 osc~ 845;\n"
        "#X obj 220 362 *~;\n"
        "#X obj 220 402 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 4 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 5 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 8 0;\n"
        "#X connect 6 1 7 0;\n"
        "#X connect 7 0 5 1;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 12 1;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 12 0 13 1;\n";

    ScDiscAudioEngine untilLoopEngine;
    untilLoopEngine.prepare (44100.0, 512, 2);

    if (! untilLoopEngine.triggerPdMessage (untilLoopPatch,
                                            "otherware_until_loop_smoke",
                                            "bang",
                                            {},
                                            0.3f,
                                            {}))
    {
        std::cerr << "Pd until loop trigger failed\n";
        return 70;
    }

    juce::AudioBuffer<float> untilLoopBuffer (2, 512);
    auto untilLoopPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        untilLoopEngine.render (untilLoopBuffer);

        for (int channel = 0; channel < untilLoopBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = untilLoopBuffer.getReadPointer (channel);

            for (int i = 0; i < untilLoopBuffer.getNumSamples(); ++i)
                untilLoopPeak = std::max (untilLoopPeak, std::abs (samples[i]));
        }
    }

    untilLoopEngine.release();

    if (untilLoopPeak <= 0.0005f)
    {
        std::cerr << "Pd until/f/select loop fixture produced no audio; peak="
                  << untilLoopPeak << '\n';
        return 71;
    }

    std::cout << "untilLoopPeak=" << untilLoopPeak << '\n';

    const juce::String listGluePatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 r otherware_list_glue_smoke;\n"
        "#X obj 48 82 t b b;\n"
        "#X obj 48 122 t b b b;\n"
        "#X msg 48 162 0.16;\n"
        "#X msg 146 162 5;\n"
        "#X msg 244 162 200;\n"
        "#X obj 48 202 pack f f f;\n"
        "#X obj 48 242 list append 999;\n"
        "#X obj 48 282 list split 3;\n"
        "#X obj 48 322 list prepend list;\n"
        "#X obj 48 362 list trim;\n"
        "#X obj 48 402 spigot 0;\n"
        "#X obj 48 442 unpack f f f;\n"
        "#X obj 48 482 pack f f f;\n"
        "#X msg 48 522 0 \\, \\$1 \\$2 \\, 0 \\$3 \\$2;\n"
        "#X obj 48 562 vline~;\n"
        "#X obj 244 522 osc~ 915;\n"
        "#X obj 244 562 *~;\n"
        "#X obj 244 602 dac~;\n"
        "#X msg 438 122 0.16 5 200 999;\n"
        "#X obj 438 162 list length;\n"
        "#X obj 438 202 select 4;\n"
        "#X msg 438 242 1;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 19 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 2 1 4 0;\n"
        "#X connect 2 2 5 0;\n"
        "#X connect 3 0 6 0;\n"
        "#X connect 4 0 6 1;\n"
        "#X connect 5 0 6 2;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 12 1 13 1;\n"
        "#X connect 12 2 13 2;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 17 1;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 16 0 18 0;\n"
        "#X connect 17 0 18 1;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 21 0 22 0;\n"
        "#X connect 22 0 11 1;\n";

    ScDiscAudioEngine listGlueEngine;
    listGlueEngine.prepare (44100.0, 512, 2);

    if (! listGlueEngine.triggerPdMessage (listGluePatch,
                                           "otherware_list_glue_smoke",
                                           "bang",
                                           {},
                                           0.3f,
                                           {}))
    {
        std::cerr << "Pd pack/list glue trigger failed\n";
        return 72;
    }

    juce::AudioBuffer<float> listGlueBuffer (2, 512);
    auto listGluePeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        listGlueEngine.render (listGlueBuffer);

        for (int channel = 0; channel < listGlueBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = listGlueBuffer.getReadPointer (channel);

            for (int i = 0; i < listGlueBuffer.getNumSamples(); ++i)
                listGluePeak = std::max (listGluePeak, std::abs (samples[i]));
        }
    }

    listGlueEngine.release();

    if (listGluePeak <= 0.0005f)
    {
        std::cerr << "Pd pack/unpack/list glue fixture produced no audio; peak="
                  << listGluePeak << '\n';
        return 73;
    }

    std::cout << "listGluePeak=" << listGluePeak << '\n';

    const juce::String routeMessagePatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 r otherware_route_message_smoke;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 48 122 list 0.18 5 210;\n"
        "#X obj 48 162 route bang float symbol list;\n"
        "#X obj 48 202 spigot 0;\n"
        "#X obj 48 242 unpack f f f;\n"
        "#X obj 48 282 pack f f f;\n"
        "#X msg 48 322 0 \\, \\$1 \\$2 \\, 0 \\$3 \\$2;\n"
        "#X obj 48 362 vline~;\n"
        "#X obj 244 322 osc~ 965;\n"
        "#X obj 244 362 *~;\n"
        "#X obj 244 402 dac~;\n"
        "#X msg 390 122 symbol open;\n"
        "#X obj 390 162 route bang float symbol list;\n"
        "#X obj 390 202 select open;\n"
        "#X msg 390 242 1;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 12 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 3 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 1 6 1;\n"
        "#X connect 5 2 6 2;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 10 1;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 10 0 11 1;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 2 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 4 1;\n";

    ScDiscAudioEngine routeMessageEngine;
    routeMessageEngine.prepare (44100.0, 512, 2);

    if (! routeMessageEngine.triggerPdMessage (routeMessagePatch,
                                               "otherware_route_message_smoke",
                                               "bang",
                                               {},
                                               0.3f,
                                               {}))
    {
        std::cerr << "Pd route message trigger failed\n";
        return 74;
    }

    juce::AudioBuffer<float> routeMessageBuffer (2, 512);
    auto routeMessagePeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        routeMessageEngine.render (routeMessageBuffer);

        for (int channel = 0; channel < routeMessageBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = routeMessageBuffer.getReadPointer (channel);

            for (int i = 0; i < routeMessageBuffer.getNumSamples(); ++i)
                routeMessagePeak = std::max (routeMessagePeak, std::abs (samples[i]));
        }
    }

    routeMessageEngine.release();

    if (routeMessagePeak <= 0.0005f)
    {
        std::cerr << "Pd route/list/symbol fixture produced no audio; peak="
                  << routeMessagePeak << '\n';
        return 75;
    }

    std::cout << "routeMessagePeak=" << routeMessagePeak << '\n';

    const juce::String messageBusPatch =
        "#N canvas 120 120 780 520 10;\n"
        "#X obj 48 42 r otherware_message_bus_smoke;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 48 122 list 0.19 5 230;\n"
        "#X obj 48 162 s otherware_message_bus_data;\n"
        "#X msg 354 122 symbol armed;\n"
        "#X obj 354 162 s otherware_message_bus_control;\n"
        "#X obj 48 222 r otherware_message_bus_data;\n"
        "#X obj 48 262 spigot 0;\n"
        "#X obj 48 302 unpack f f f;\n"
        "#X obj 48 342 pack f f f;\n"
        "#X msg 48 382 0 \\, \\$1 \\$2 \\, 0 \\$3 \\$2;\n"
        "#X obj 48 422 vline~;\n"
        "#X obj 260 382 osc~ 1015;\n"
        "#X obj 260 422 *~;\n"
        "#X obj 260 462 dac~;\n"
        "#X obj 474 222 r otherware_message_bus_control;\n"
        "#X obj 474 262 select armed;\n"
        "#X msg 474 302 1;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 4 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 1 9 1;\n"
        "#X connect 8 2 9 2;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 13 1;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 13 0 14 1;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 7 1;\n";

    ScDiscAudioEngine messageBusEngine;
    messageBusEngine.prepare (44100.0, 512, 2);

    if (! messageBusEngine.triggerPdMessage (messageBusPatch,
                                             "otherware_message_bus_smoke",
                                             "bang",
                                             {},
                                             0.3f,
                                             {}))
    {
        std::cerr << "Pd multi-selector send/receive trigger failed\n";
        return 76;
    }

    juce::AudioBuffer<float> messageBusBuffer (2, 512);
    auto messageBusPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        messageBusEngine.render (messageBusBuffer);

        for (int channel = 0; channel < messageBusBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = messageBusBuffer.getReadPointer (channel);

            for (int i = 0; i < messageBusBuffer.getNumSamples(); ++i)
                messageBusPeak = std::max (messageBusPeak, std::abs (samples[i]));
        }
    }

    messageBusEngine.release();

    if (messageBusPeak <= 0.0005f)
    {
        std::cerr << "Pd multi-selector send/receive fixture produced no audio; peak="
                  << messageBusPeak << '\n';
        return 77;
    }

    std::cout << "messageBusPeak=" << messageBusPeak << '\n';

    const juce::String controlMathPatch =
        "#N canvas 120 120 720 480 10;\n"
        "#X obj 48 42 r otherware_control_math_smoke;\n"
        "#X msg 48 82 60;\n"
        "#X obj 48 122 + 12;\n"
        "#X obj 48 162 * 2;\n"
        "#X obj 48 202 / 2;\n"
        "#X obj 48 242 - 3;\n"
        "#X obj 48 282 clip 0 127;\n"
        "#X obj 48 322 mtof;\n"
        "#X obj 48 362 osc~;\n"
        "#X obj 48 402 *~ 0.2;\n"
        "#X obj 48 442 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 10 1;\n";

    ScDiscAudioEngine controlMathEngine;
    controlMathEngine.prepare (44100.0, 512, 2);

    if (! controlMathEngine.triggerPdMessage (controlMathPatch,
                                              "otherware_control_math_smoke",
                                              "bang",
                                              {},
                                              0.3f,
                                              {}))
    {
        std::cerr << "Pd control math trigger failed\n";
        return 78;
    }

    juce::AudioBuffer<float> controlMathBuffer (2, 512);
    auto controlMathPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        controlMathEngine.render (controlMathBuffer);

        for (int channel = 0; channel < controlMathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = controlMathBuffer.getReadPointer (channel);

            for (int i = 0; i < controlMathBuffer.getNumSamples(); ++i)
                controlMathPeak = std::max (controlMathPeak, std::abs (samples[i]));
        }
    }

    controlMathEngine.release();

    if (controlMathPeak <= 0.0005f)
    {
        std::cerr << "Pd arithmetic/clip/mtof fixture produced no audio; peak="
                  << controlMathPeak << '\n';
        return 79;
    }

    std::cout << "controlMathPeak=" << controlMathPeak << '\n';

    const juce::String controlLogicPatch =
        "#N canvas 120 120 760 520 10;\n"
        "#X obj 48 42 r otherware_control_logic_smoke;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 372 122 72;\n"
        "#X obj 372 162 == 72;\n"
        "#X obj 372 202 != 0;\n"
        "#X obj 372 242 >= 1;\n"
        "#X obj 372 282 <= 1;\n"
        "#X obj 372 322 && 1;\n"
        "#X obj 372 362 || 0;\n"
        "#X obj 372 402 select 1;\n"
        "#X msg 372 442 1;\n"
        "#X msg 48 122 0.18 5 220;\n"
        "#X obj 48 162 spigot 0;\n"
        "#X obj 48 202 unpack f f f;\n"
        "#X obj 48 242 pack f f f;\n"
        "#X msg 48 282 0 \\, \\$1 \\$2 \\, 0 \\$3 \\$2;\n"
        "#X obj 48 322 vline~;\n"
        "#X obj 244 282 osc~ 1065;\n"
        "#X obj 244 322 *~;\n"
        "#X obj 244 362 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 11 0;\n"
        "#X connect 1 1 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 12 1;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 13 1 14 1;\n"
        "#X connect 13 2 14 2;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 16 0 18 1;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 18 0 19 0;\n"
        "#X connect 18 0 19 1;\n";

    ScDiscAudioEngine controlLogicEngine;
    controlLogicEngine.prepare (44100.0, 512, 2);

    if (! controlLogicEngine.triggerPdMessage (controlLogicPatch,
                                               "otherware_control_logic_smoke",
                                               "bang",
                                               {},
                                               0.3f,
                                               {}))
    {
        std::cerr << "Pd control logic trigger failed\n";
        return 80;
    }

    juce::AudioBuffer<float> controlLogicBuffer (2, 512);
    auto controlLogicPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        controlLogicEngine.render (controlLogicBuffer);

        for (int channel = 0; channel < controlLogicBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = controlLogicBuffer.getReadPointer (channel);

            for (int i = 0; i < controlLogicBuffer.getNumSamples(); ++i)
                controlLogicPeak = std::max (controlLogicPeak, std::abs (samples[i]));
        }
    }

    controlLogicEngine.release();

    if (controlLogicPeak <= 0.0005f)
    {
        std::cerr << "Pd comparison/boolean fixture produced no audio; peak="
                  << controlLogicPeak << '\n';
        return 81;
    }

    std::cout << "controlLogicPeak=" << controlLogicPeak << '\n';

    const juce::String integerMathPatch =
        "#N canvas 120 120 760 520 10;\n"
        "#X obj 48 42 r otherware_integer_math_smoke;\n"
        "#X msg 48 82 42;\n"
        "#X obj 48 122 % 5;\n"
        "#X obj 48 162 + 70;\n"
        "#X obj 184 122 mod 7;\n"
        "#X obj 184 162 div 2;\n"
        "#X obj 184 202 << 1;\n"
        "#X obj 184 242 >> 1;\n"
        "#X obj 184 282 & 3;\n"
        "#X obj 184 322 | 0;\n"
        "#X obj 48 202 +;\n"
        "#X obj 48 242 clip 0 127;\n"
        "#X obj 48 282 mtof;\n"
        "#X obj 48 322 osc~;\n"
        "#X obj 48 362 *~ 0.2;\n"
        "#X obj 48 402 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 4 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 10 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 1;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 14 0 15 1;\n";

    ScDiscAudioEngine integerMathEngine;
    integerMathEngine.prepare (44100.0, 512, 2);

    if (! integerMathEngine.triggerPdMessage (integerMathPatch,
                                              "otherware_integer_math_smoke",
                                              "bang",
                                              {},
                                              0.3f,
                                              {}))
    {
        std::cerr << "Pd integer math trigger failed\n";
        return 82;
    }

    juce::AudioBuffer<float> integerMathBuffer (2, 512);
    auto integerMathPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        integerMathEngine.render (integerMathBuffer);

        for (int channel = 0; channel < integerMathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = integerMathBuffer.getReadPointer (channel);

            for (int i = 0; i < integerMathBuffer.getNumSamples(); ++i)
                integerMathPeak = std::max (integerMathPeak, std::abs (samples[i]));
        }
    }

    integerMathEngine.release();

    if (integerMathPeak <= 0.0005f)
    {
        std::cerr << "Pd modulo/div/bitwise fixture produced no audio; peak="
                  << integerMathPeak << '\n';
        return 83;
    }

    std::cout << "integerMathPeak=" << integerMathPeak << '\n';

    const juce::String advancedMathPatch =
        "#N canvas 120 120 840 560 10;\n"
        "#X obj 48 42 r otherware_advanced_math_smoke;\n"
        "#X msg 48 82 4;\n"
        "#X obj 48 122 pow 2;\n"
        "#X obj 48 162 sqrt;\n"
        "#X obj 48 202 log;\n"
        "#X obj 48 242 exp;\n"
        "#X obj 48 282 abs;\n"
        "#X obj 48 322 sin;\n"
        "#X obj 48 362 cos;\n"
        "#X obj 48 402 atan;\n"
        "#X obj 48 442 + 72;\n"
        "#X obj 48 482 clip 0 127;\n"
        "#X obj 48 522 mtof;\n"
        "#X obj 48 562 osc~;\n"
        "#X obj 48 602 *~ 0.2;\n"
        "#X obj 48 642 dac~;\n"
        "#X msg 338 82 86;\n"
        "#X obj 338 122 dbtorms;\n"
        "#X obj 338 162 rmstodb;\n"
        "#X obj 338 202 - 80;\n"
        "#X obj 338 242 wrap 0 12;\n"
        "#X obj 338 282 + 60;\n"
        "#X obj 338 322 mtof;\n"
        "#X obj 338 362 osc~;\n"
        "#X obj 338 402 *~ 0.12;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 16 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 14 0 15 1;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 16 0 18 0;\n"
        "#X connect 18 0 19 0;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 21 0 22 0;\n"
        "#X connect 22 0 23 0;\n"
        "#X connect 23 0 24 0;\n"
        "#X connect 24 0 15 0;\n"
        "#X connect 24 0 15 1;\n";

    ScDiscAudioEngine advancedMathEngine;
    advancedMathEngine.prepare (44100.0, 512, 2);

    if (! advancedMathEngine.triggerPdMessage (advancedMathPatch,
                                               "otherware_advanced_math_smoke",
                                               "bang",
                                               {},
                                               0.3f,
                                               {}))
    {
        std::cerr << "Pd advanced math trigger failed\n";
        return 84;
    }

    juce::AudioBuffer<float> advancedMathBuffer (2, 512);
    auto advancedMathPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        advancedMathEngine.render (advancedMathBuffer);

        for (int channel = 0; channel < advancedMathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = advancedMathBuffer.getReadPointer (channel);

            for (int i = 0; i < advancedMathBuffer.getNumSamples(); ++i)
                advancedMathPeak = std::max (advancedMathPeak, std::abs (samples[i]));
        }
    }

    advancedMathEngine.release();

    if (advancedMathPeak <= 0.0005f)
    {
        std::cerr << "Pd transcendental/conversion math fixture produced no audio; peak="
                  << advancedMathPeak << '\n';
        return 85;
    }

    std::cout << "advancedMathPeak=" << advancedMathPeak << '\n';

    const juce::String signalMathPatch =
        "#N canvas 120 120 900 620 10;\n"
        "#X obj 48 42 r otherware_signal_math_smoke;\n"
        "#X msg 48 82 0.25;\n"
        "#X obj 48 122 sig~;\n"
        "#X obj 48 162 +~ 0.5;\n"
        "#X obj 48 202 -~ 0.2;\n"
        "#X obj 48 242 *~ 0.8;\n"
        "#X obj 48 282 /~ 2;\n"
        "#X obj 48 322 max~ 0.1;\n"
        "#X obj 48 362 min~ 0.8;\n"
        "#X obj 48 402 clip~ 0 0.5;\n"
        "#X obj 48 442 wrap~;\n"
        "#X obj 48 482 abs~;\n"
        "#X obj 48 522 sqrt~;\n"
        "#X obj 48 562 *~ 0.2;\n"
        "#X obj 48 602 dac~;\n"
        "#X obj 308 122 sig~ 0.25;\n"
        "#X obj 308 162 cos~;\n"
        "#X obj 308 202 *~ 0.08;\n"
        "#X obj 468 122 sig~ 0.3;\n"
        "#X obj 468 162 log~;\n"
        "#X obj 468 202 exp~;\n"
        "#X obj 468 242 *~ 0.05;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 13 0 14 1;\n"
        "#X connect 17 0 14 0;\n"
        "#X connect 17 0 14 1;\n"
        "#X connect 20 0 14 0;\n"
        "#X connect 20 0 14 1;\n";

    ScDiscAudioEngine signalMathEngine;
    signalMathEngine.prepare (44100.0, 512, 2);

    if (! signalMathEngine.triggerPdMessage (signalMathPatch,
                                             "otherware_signal_math_smoke",
                                             "bang",
                                             {},
                                             0.3f,
                                             {}))
    {
        std::cerr << "Pd signal math trigger failed\n";
        return 86;
    }

    juce::AudioBuffer<float> signalMathBuffer (2, 512);
    auto signalMathPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        signalMathEngine.render (signalMathBuffer);

        for (int channel = 0; channel < signalMathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = signalMathBuffer.getReadPointer (channel);

            for (int i = 0; i < signalMathBuffer.getNumSamples(); ++i)
                signalMathPeak = std::max (signalMathPeak, std::abs (samples[i]));
        }
    }

    signalMathEngine.release();

    if (signalMathPeak <= 0.0005f)
    {
        std::cerr << "Pd signal-rate arithmetic/transcendental fixture produced no audio; peak="
                  << signalMathPeak << '\n';
        return 87;
    }

    std::cout << "signalMathPeak=" << signalMathPeak << '\n';

    DiscAudioTrigger pdSnakeTrigger;
    pdSnakeTrigger.depth = 1;
    pdSnakeTrigger.branchIndex = 39;
    pdSnakeTrigger.pdDurationSeconds = 0.3f;
    pdSnakeTrigger.pdPatch =
        "#N canvas 120 120 620 360 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.18 5 \\, 0 210 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 490;\n"
        "#X obj 314 82 osc~ 735;\n"
        "#X obj 184 132 *~;\n"
        "#X obj 314 132 *~;\n"
        "#X obj 238 188 snake~ in 2;\n"
        "#X obj 238 238 snake~ out 2;\n"
        "#X obj 238 288 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 5 1;\n"
        "#X connect 2 0 6 1;\n"
        "#X connect 3 0 5 0;\n"
        "#X connect 4 0 6 0;\n"
        "#X connect 5 0 7 0;\n"
        "#X connect 6 0 7 1;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 1 9 1;\n";

    ScDiscAudioEngine snakeEngine;
    snakeEngine.prepare (44100.0, 512, 2);
    snakeEngine.trigger (pdSnakeTrigger);

    juce::AudioBuffer<float> snakeBuffer (2, 512);
    auto snakeLeftPeak = 0.0f;
    auto snakeRightPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        snakeEngine.render (snakeBuffer);

        const auto* left = snakeBuffer.getReadPointer (0);
        const auto* right = snakeBuffer.getReadPointer (1);

        for (int i = 0; i < snakeBuffer.getNumSamples(); ++i)
        {
            snakeLeftPeak = std::max (snakeLeftPeak, std::abs (left[i]));
            snakeRightPeak = std::max (snakeRightPeak, std::abs (right[i]));
        }
    }

    snakeEngine.release();

    if (snakeLeftPeak <= 0.0005f || snakeRightPeak <= 0.0005f)
    {
        std::cerr << "Pd snake~ in/out fixture produced no stereo audio; left="
                  << snakeLeftPeak << " right=" << snakeRightPeak << '\n';
        return 90;
    }

    std::cout << "snakePeak=" << std::max (snakeLeftPeak, snakeRightPeak) << '\n';

    DiscAudioTrigger pdThrowCatchTrigger;
    pdThrowCatchTrigger.depth = 1;
    pdThrowCatchTrigger.branchIndex = 29;
    pdThrowCatchTrigger.pdDurationSeconds = 0.3f;
    pdThrowCatchTrigger.pdPatch =
        "#N canvas 120 120 560 340 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X msg 48 82 0 \\, 0.14 5 \\, 0 210 5;\n"
        "#X obj 48 122 vline~;\n"
        "#X obj 184 82 osc~ 1040;\n"
        "#X obj 184 122 *~;\n"
        "#X obj 184 162 throw~ otherware_mix_bus;\n"
        "#X obj 364 122 catch~ otherware_mix_bus;\n"
        "#X obj 364 162 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine throwCatchEngine;
    throwCatchEngine.prepare (44100.0, 512, 2);
    throwCatchEngine.trigger (pdThrowCatchTrigger);

    juce::AudioBuffer<float> throwCatchBuffer (2, 512);
    auto throwCatchPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        throwCatchEngine.render (throwCatchBuffer);

        for (int channel = 0; channel < throwCatchBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = throwCatchBuffer.getReadPointer (channel);

            for (int i = 0; i < throwCatchBuffer.getNumSamples(); ++i)
                throwCatchPeak = std::max (throwCatchPeak, std::abs (samples[i]));
        }
    }

    throwCatchEngine.release();

    if (throwCatchPeak <= 0.0005f)
    {
        std::cerr << "Pd throw~/catch~ fixture produced no audio; peak="
                  << throwCatchPeak << '\n';
        return 21;
    }

    std::cout << "throwCatchPeak=" << throwCatchPeak << '\n';

    DiscAudioTrigger pdArrayReadTrigger;
    pdArrayReadTrigger.depth = 1;
    pdArrayReadTrigger.branchIndex = 17;
    pdArrayReadTrigger.pdDurationSeconds = 0.3f;
    pdArrayReadTrigger.pdPatch =
        "#N canvas 120 120 560 340 10;\n"
        "#X obj 48 42 sig~ 3;\n"
        "#X obj 48 82 tabread4~ otherware_wave;\n"
        "#X obj 48 122 *~ 0.2;\n"
        "#X obj 48 162 dac~;\n"
        "#X array otherware_wave 11 float 3;\n"
        "#A 0 0 0.4 0.8 0.4 0 -0.4 -0.8 -0.4 0 0.4 0.8;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 2 0 3 1;\n";

    ScDiscAudioEngine arrayReadEngine;
    arrayReadEngine.prepare (44100.0, 512, 2);
    arrayReadEngine.trigger (pdArrayReadTrigger);

    juce::AudioBuffer<float> arrayReadBuffer (2, 512);
    auto arrayReadPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        arrayReadEngine.render (arrayReadBuffer);

        for (int channel = 0; channel < arrayReadBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = arrayReadBuffer.getReadPointer (channel);

            for (int i = 0; i < arrayReadBuffer.getNumSamples(); ++i)
                arrayReadPeak = std::max (arrayReadPeak, std::abs (samples[i]));
        }
    }

    arrayReadEngine.release();

    if (arrayReadPeak <= 0.0005f)
    {
        std::cerr << "Pd array/tabread4~ fixture produced no audio; peak="
                  << arrayReadPeak << '\n';
        return 22;
    }

    std::cout << "arrayReadPeak=" << arrayReadPeak << '\n';

    DiscAudioTrigger pdRuntimeArrayTrigger;
    pdRuntimeArrayTrigger.depth = 1;
    pdRuntimeArrayTrigger.branchIndex = 18;
    pdRuntimeArrayTrigger.pdDurationSeconds = 0.3f;
    pdRuntimeArrayTrigger.pdPatch =
        "#N canvas 120 120 640 380 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 \\; otherware_runtime_wave resize 8 \\; otherware_runtime_wave 0 0 0.9 0.7 0.5 0.3 0.1 0;\n"
        "#X obj 352 42 array define otherware_runtime_wave 8;\n"
        "#X obj 48 142 sig~ 2;\n"
        "#X obj 48 182 tabread4~ otherware_runtime_wave;\n"
        "#X obj 48 222 *~ 0.2;\n"
        "#X obj 48 262 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine runtimeArrayEngine;
    runtimeArrayEngine.prepare (44100.0, 512, 2);
    runtimeArrayEngine.trigger (pdRuntimeArrayTrigger);

    juce::AudioBuffer<float> runtimeArrayBuffer (2, 512);
    auto runtimeArrayPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        runtimeArrayEngine.render (runtimeArrayBuffer);

        for (int channel = 0; channel < runtimeArrayBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = runtimeArrayBuffer.getReadPointer (channel);

            for (int i = 0; i < runtimeArrayBuffer.getNumSamples(); ++i)
                runtimeArrayPeak = std::max (runtimeArrayPeak, std::abs (samples[i]));
        }
    }

    runtimeArrayEngine.release();

    if (runtimeArrayPeak <= 0.0005f)
    {
        std::cerr << "Pd runtime array write/read fixture produced no audio; peak="
                  << runtimeArrayPeak << '\n';
        return 23;
    }

    std::cout << "runtimeArrayPeak=" << runtimeArrayPeak << '\n';

    DiscAudioTrigger pdArrayOpsTrigger;
    pdArrayOpsTrigger.depth = 1;
    pdArrayOpsTrigger.branchIndex = 30;
    pdArrayOpsTrigger.pdDurationSeconds = 0.3f;
    pdArrayOpsTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X obj 48 82 t b b b;\n"
        "#X msg 368 82 61 64 67 72 76;\n"
        "#X obj 368 122 array set otherware_array_ops;\n"
        "#X msg 48 122 0 5;\n"
        "#X obj 48 162 array get otherware_array_ops;\n"
        "#X obj 48 202 list split 1;\n"
        "#X obj 48 242 mtof;\n"
        "#X obj 48 282 osc~;\n"
        "#X obj 48 322 *~ 0.16;\n"
        "#X obj 48 362 dac~;\n"
        "#X obj 224 122 array size otherware_array_ops;\n"
        "#X obj 224 162 + 64;\n"
        "#X obj 224 202 mtof;\n"
        "#X obj 224 242 osc~;\n"
        "#X obj 224 282 *~ 0.08;\n"
        "#X obj 540 42 array define otherware_array_ops 8;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 4 0;\n"
        "#X connect 1 1 11 0;\n"
        "#X connect 1 2 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 10 1;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 10 0;\n"
        "#X connect 15 0 10 1;\n";

    ScDiscAudioEngine arrayOpsEngine;
    arrayOpsEngine.prepare (44100.0, 512, 2);
    arrayOpsEngine.trigger (pdArrayOpsTrigger);

    juce::AudioBuffer<float> arrayOpsBuffer (2, 512);
    auto arrayOpsPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        arrayOpsEngine.render (arrayOpsBuffer);

        for (int channel = 0; channel < arrayOpsBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = arrayOpsBuffer.getReadPointer (channel);

            for (int i = 0; i < arrayOpsBuffer.getNumSamples(); ++i)
                arrayOpsPeak = std::max (arrayOpsPeak, std::abs (samples[i]));
        }
    }

    arrayOpsEngine.release();

    if (arrayOpsPeak <= 0.0005f)
    {
        std::cerr << "Pd array get/set/size fixture produced no audio; peak="
                  << arrayOpsPeak << '\n';
        return 88;
    }

    std::cout << "arrayOpsPeak=" << arrayOpsPeak << '\n';

    DiscAudioTrigger pdArrayQueryTrigger;
    pdArrayQueryTrigger.depth = 1;
    pdArrayQueryTrigger.branchIndex = 31;
    pdArrayQueryTrigger.pdDurationSeconds = 0.3f;
    pdArrayQueryTrigger.pdPatch =
        "#N canvas 120 120 900 560 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X obj 48 82 t b b b b b b;\n"
        "#X msg 612 122 0.1 0.2 0.5 0.1 0.1;\n"
        "#X obj 612 162 array set otherware_array_queries;\n"
        "#X obj 48 122 array sum otherware_array_queries;\n"
        "#X obj 48 162 + 60;\n"
        "#X obj 48 202 mtof;\n"
        "#X obj 48 242 osc~;\n"
        "#X obj 48 282 *~ 0.05;\n"
        "#X obj 188 122 array max otherware_array_queries;\n"
        "#X obj 188 162 + 60;\n"
        "#X obj 188 202 mtof;\n"
        "#X obj 188 242 osc~;\n"
        "#X obj 188 282 *~ 0.05;\n"
        "#X obj 328 122 array min otherware_array_queries;\n"
        "#X obj 328 162 + 72;\n"
        "#X obj 328 202 mtof;\n"
        "#X obj 328 242 osc~;\n"
        "#X obj 328 282 *~ 0.05;\n"
        "#X msg 468 122 0.5;\n"
        "#X obj 468 162 array quantile otherware_array_queries;\n"
        "#X obj 468 202 + 67;\n"
        "#X obj 468 242 mtof;\n"
        "#X obj 468 282 osc~;\n"
        "#X obj 468 322 *~ 0.05;\n"
        "#X obj 612 222 array random otherware_array_queries;\n"
        "#X obj 612 262 + 74;\n"
        "#X obj 612 302 mtof;\n"
        "#X obj 612 342 osc~;\n"
        "#X obj 612 382 *~ 0.05;\n"
        "#X obj 744 42 array define otherware_array_queries 8;\n"
        "#X obj 48 432 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 4 0;\n"
        "#X connect 1 1 9 0;\n"
        "#X connect 1 2 14 0;\n"
        "#X connect 1 3 19 0;\n"
        "#X connect 1 4 25 0;\n"
        "#X connect 1 5 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 31 0;\n"
        "#X connect 8 0 31 1;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 31 0;\n"
        "#X connect 13 0 31 1;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 18 0 31 0;\n"
        "#X connect 18 0 31 1;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 21 0 22 0;\n"
        "#X connect 22 0 23 0;\n"
        "#X connect 23 0 24 0;\n"
        "#X connect 24 0 31 0;\n"
        "#X connect 24 0 31 1;\n"
        "#X connect 25 0 26 0;\n"
        "#X connect 26 0 27 0;\n"
        "#X connect 27 0 28 0;\n"
        "#X connect 28 0 29 0;\n"
        "#X connect 29 0 31 0;\n"
        "#X connect 29 0 31 1;\n";

    ScDiscAudioEngine arrayQueryEngine;
    arrayQueryEngine.prepare (44100.0, 512, 2);
    arrayQueryEngine.trigger (pdArrayQueryTrigger);

    juce::AudioBuffer<float> arrayQueryBuffer (2, 512);
    auto arrayQueryPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        arrayQueryEngine.render (arrayQueryBuffer);

        for (int channel = 0; channel < arrayQueryBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = arrayQueryBuffer.getReadPointer (channel);

            for (int i = 0; i < arrayQueryBuffer.getNumSamples(); ++i)
                arrayQueryPeak = std::max (arrayQueryPeak, std::abs (samples[i]));
        }
    }

    arrayQueryEngine.release();

    if (arrayQueryPeak <= 0.0005f)
    {
        std::cerr << "Pd array random/max/min/sum/quantile fixture produced no audio; peak="
                  << arrayQueryPeak << '\n';
        return 89;
    }

    std::cout << "arrayQueryPeak=" << arrayQueryPeak << '\n';

    DiscAudioTrigger pdTextGetTrigger;
    pdTextGetTrigger.depth = 1;
    pdTextGetTrigger.branchIndex = 19;
    pdTextGetTrigger.pdDurationSeconds = 0.3f;
    pdTextGetTrigger.pdPatch =
        "#N canvas 120 120 640 420 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 0;\n"
        "#X obj 48 122 text get otherware_text_steps;\n"
        "#X obj 48 162 unpack f;\n"
        "#X obj 48 202 mtof;\n"
        "#X obj 48 242 osc~;\n"
        "#X obj 48 282 *~ 0.2;\n"
        "#X obj 48 322 dac~;\n"
        "#X obj 352 42 text define -k otherware_text_steps;\n"
        "#A set 69 \\;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine textGetEngine;
    textGetEngine.prepare (44100.0, 512, 2);
    textGetEngine.trigger (pdTextGetTrigger);

    juce::AudioBuffer<float> textGetBuffer (2, 512);
    auto textGetPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        textGetEngine.render (textGetBuffer);

        for (int channel = 0; channel < textGetBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = textGetBuffer.getReadPointer (channel);

            for (int i = 0; i < textGetBuffer.getNumSamples(); ++i)
                textGetPeak = std::max (textGetPeak, std::abs (samples[i]));
        }
    }

    textGetEngine.release();

    if (textGetPeak <= 0.0005f)
    {
        std::cerr << "Pd text define/text get fixture produced no audio; peak="
                  << textGetPeak << '\n';
        return 24;
    }

    std::cout << "textGetPeak=" << textGetPeak << '\n';

    DiscAudioTrigger pdTextSetTrigger;
    pdTextSetTrigger.depth = 1;
    pdTextSetTrigger.branchIndex = 20;
    pdTextSetTrigger.pdDurationSeconds = 0.3f;
    pdTextSetTrigger.pdPatch =
        "#N canvas 120 120 680 440 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 72;\n"
        "#X msg 128 82 0;\n"
        "#X obj 48 122 text set otherware_dynamic_text;\n"
        "#X obj 48 172 loadbang;\n"
        "#X msg 48 212 0;\n"
        "#X obj 48 252 text get otherware_dynamic_text;\n"
        "#X obj 48 292 unpack f;\n"
        "#X obj 48 332 mtof;\n"
        "#X obj 48 372 osc~;\n"
        "#X obj 192 372 *~ 0.2;\n"
        "#X obj 192 412 dac~;\n"
        "#X obj 396 42 text define otherware_dynamic_text;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 2 0;\n"
        "#X connect 1 0 3 0;\n"
        "#X connect 2 0 3 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 10 0 11 1;\n";

    ScDiscAudioEngine textSetEngine;
    textSetEngine.prepare (44100.0, 512, 2);
    textSetEngine.trigger (pdTextSetTrigger);

    juce::AudioBuffer<float> textSetBuffer (2, 512);
    auto textSetPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        textSetEngine.render (textSetBuffer);

        for (int channel = 0; channel < textSetBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = textSetBuffer.getReadPointer (channel);

            for (int i = 0; i < textSetBuffer.getNumSamples(); ++i)
                textSetPeak = std::max (textSetPeak, std::abs (samples[i]));
        }
    }

    textSetEngine.release();

    if (textSetPeak <= 0.0005f)
    {
        std::cerr << "Pd dynamic text set/get fixture produced no audio; peak="
                  << textSetPeak << '\n';
        return 25;
    }

    std::cout << "textSetPeak=" << textSetPeak << '\n';

    DiscAudioTrigger pdTextSequenceTrigger;
    pdTextSequenceTrigger.depth = 1;
    pdTextSequenceTrigger.branchIndex = 21;
    pdTextSequenceTrigger.pdDurationSeconds = 0.3f;
    pdTextSequenceTrigger.pdPatch =
        "#N canvas 120 120 680 440 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 line 0 \\; bang;\n"
        "#X obj 48 142 text sequence otherware_sequence_text;\n"
        "#X obj 48 182 unpack f;\n"
        "#X obj 48 222 mtof;\n"
        "#X obj 48 262 osc~;\n"
        "#X obj 48 302 *~ 0.2;\n"
        "#X obj 48 342 dac~;\n"
        "#X obj 376 42 text define -k otherware_sequence_text;\n"
        "#A set 76 \\;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine textSequenceEngine;
    textSequenceEngine.prepare (44100.0, 512, 2);
    textSequenceEngine.trigger (pdTextSequenceTrigger);

    juce::AudioBuffer<float> textSequenceBuffer (2, 512);
    auto textSequencePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        textSequenceEngine.render (textSequenceBuffer);

        for (int channel = 0; channel < textSequenceBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = textSequenceBuffer.getReadPointer (channel);

            for (int i = 0; i < textSequenceBuffer.getNumSamples(); ++i)
                textSequencePeak = std::max (textSequencePeak, std::abs (samples[i]));
        }
    }

    textSequenceEngine.release();

    if (textSequencePeak <= 0.0005f)
    {
        std::cerr << "Pd text sequence fixture produced no audio; peak="
                  << textSequencePeak << '\n';
        return 26;
    }

    std::cout << "textSequencePeak=" << textSequencePeak << '\n';

    DiscAudioTrigger pdTextOpsTrigger;
    pdTextOpsTrigger.depth = 1;
    pdTextOpsTrigger.branchIndex = 53;
    pdTextOpsTrigger.pdDurationSeconds = 0.35f;
    pdTextOpsTrigger.pdPatch =
        "#N canvas 120 120 860 560 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X obj 48 82 t b b b b b;\n"
        "#X msg 48 122 63;\n"
        "#X obj 48 162 text search otherware_text_ops 0 >;\n"
        "#X msg 210 122 1;\n"
        "#X obj 210 162 text delete otherware_text_ops;\n"
        "#X obj 392 122 del 10;\n"
        "#X msg 392 162 1;\n"
        "#X msg 472 162 65;\n"
        "#X obj 472 202 text insert otherware_text_ops;\n"
        "#X obj 620 122 del 20;\n"
        "#X obj 620 162 text tolist otherware_text_ops;\n"
        "#X obj 620 202 list length;\n"
        "#X obj 210 242 del 30;\n"
        "#X msg 210 282 1;\n"
        "#X obj 210 322 text get otherware_text_ops;\n"
        "#X obj 210 362 unpack f;\n"
        "#X obj 210 402 mtof;\n"
        "#X obj 210 442 osc~;\n"
        "#X obj 356 442 *~ 0.17;\n"
        "#X obj 356 482 dac~;\n"
        "#X obj 620 242 del 40;\n"
        "#X msg 620 282 74 76;\n"
        "#X obj 620 322 text fromlist otherware_text_ops;\n"
        "#X obj 48 582 text define -k otherware_text_ops;\n"
        "#A set 60 \\; 64 \\; 67 \\;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 4 0;\n"
        "#X connect 1 2 6 0;\n"
        "#X connect 1 3 10 0;\n"
        "#X connect 1 4 13 0;\n"
        "#X connect 1 4 21 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 8 0;\n"
        "#X connect 7 0 9 1;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 18 0 19 0;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 19 0 20 1;\n"
        "#X connect 21 0 22 0;\n"
        "#X connect 22 0 23 0;\n";

    ScDiscAudioEngine textOpsEngine;
    textOpsEngine.prepare (44100.0, 512, 2);
    textOpsEngine.trigger (pdTextOpsTrigger);

    juce::AudioBuffer<float> textOpsBuffer (2, 512);
    auto textOpsPeak = 0.0f;

    for (int block = 0; block < 100; ++block)
    {
        textOpsEngine.render (textOpsBuffer);

        for (int channel = 0; channel < textOpsBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = textOpsBuffer.getReadPointer (channel);

            for (int i = 0; i < textOpsBuffer.getNumSamples(); ++i)
                textOpsPeak = std::max (textOpsPeak, std::abs (samples[i]));
        }
    }

    textOpsEngine.release();

    if (textOpsPeak <= 0.0005f)
    {
        std::cerr << "Pd text insert/delete/search/list fixture produced no audio; peak="
                  << textOpsPeak << '\n';
        return 107;
    }

    std::cout << "textOpsPeak=" << textOpsPeak << '\n';

    DiscAudioTrigger pdTextfileTrigger;
    pdTextfileTrigger.depth = 1;
    pdTextfileTrigger.branchIndex = 22;
    pdTextfileTrigger.pdDurationSeconds = 0.3f;
    pdTextfileTrigger.pdPatch =
        "#N canvas 120 120 720 460 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 clear \\, add 79 \\, rewind \\, bang;\n"
        "#X obj 48 142 textfile;\n"
        "#X obj 48 182 unpack f;\n"
        "#X obj 48 222 mtof;\n"
        "#X obj 48 262 osc~;\n"
        "#X obj 48 302 *~ 0.2;\n"
        "#X obj 48 342 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine textfileEngine;
    textfileEngine.prepare (44100.0, 512, 2);
    textfileEngine.trigger (pdTextfileTrigger);

    juce::AudioBuffer<float> textfileBuffer (2, 512);
    auto textfilePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        textfileEngine.render (textfileBuffer);

        for (int channel = 0; channel < textfileBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = textfileBuffer.getReadPointer (channel);

            for (int i = 0; i < textfileBuffer.getNumSamples(); ++i)
                textfilePeak = std::max (textfilePeak, std::abs (samples[i]));
        }
    }

    textfileEngine.release();

    if (textfilePeak <= 0.0005f)
    {
        std::cerr << "Pd textfile fixture produced no audio; peak="
                  << textfilePeak << '\n';
        return 27;
    }

    std::cout << "textfilePeak=" << textfilePeak << '\n';

    DiscAudioTrigger pdQlistTrigger;
    pdQlistTrigger.depth = 1;
    pdQlistTrigger.branchIndex = 23;
    pdQlistTrigger.pdDurationSeconds = 0.3f;
    pdQlistTrigger.pdPatch =
        "#N canvas 120 120 760 480 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 clear \\, add 0 otherware_qlist_pitch 81 \\, rewind \\, bang;\n"
        "#X obj 48 142 qlist;\n"
        "#X obj 360 82 r otherware_qlist_pitch;\n"
        "#X obj 360 122 mtof;\n"
        "#X obj 360 162 osc~;\n"
        "#X obj 360 202 *~ 0.2;\n"
        "#X obj 360 242 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 6 0 7 1;\n";

    ScDiscAudioEngine qlistEngine;
    qlistEngine.prepare (44100.0, 512, 2);
    qlistEngine.trigger (pdQlistTrigger);

    juce::AudioBuffer<float> qlistBuffer (2, 512);
    auto qlistPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        qlistEngine.render (qlistBuffer);

        for (int channel = 0; channel < qlistBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = qlistBuffer.getReadPointer (channel);

            for (int i = 0; i < qlistBuffer.getNumSamples(); ++i)
                qlistPeak = std::max (qlistPeak, std::abs (samples[i]));
        }
    }

    qlistEngine.release();

    if (qlistPeak <= 0.0005f)
    {
        std::cerr << "Pd qlist fixture produced no audio; peak="
                  << qlistPeak << '\n';
        return 28;
    }

    std::cout << "qlistPeak=" << qlistPeak << '\n';

    DiscAudioTrigger pdFilePathTrigger;
    pdFilePathTrigger.depth = 1;
    pdFilePathTrigger.branchIndex = 41;
    pdFilePathTrigger.pdDurationSeconds = 0.3f;
    pdFilePathTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b b b;\n"
        "#X msg 48 122 symbol /tmp;\n"
        "#X obj 48 162 file isabsolute;\n"
        "#X obj 48 202 sel 1;\n"
        "#X obj 48 242 s otherware_file_path_ok;\n"
        "#X msg 210 122 symbol /tmp/example.wav;\n"
        "#X obj 210 162 file splitext;\n"
        "#X obj 210 202 list length;\n"
        "#X obj 210 242 sel 2;\n"
        "#X obj 210 282 s otherware_file_path_ok;\n"
        "#X msg 414 122 list /tmp otherware.txt;\n"
        "#X obj 414 162 file join;\n"
        "#X obj 414 202 list fromsymbol;\n"
        "#X obj 414 242 list length;\n"
        "#X obj 414 282 > 4;\n"
        "#X obj 414 322 sel 1;\n"
        "#X obj 414 362 s otherware_file_path_ok;\n"
        "#X msg 604 122 symbol ./alpha/../beta;\n"
        "#X obj 604 162 file normalize;\n"
        "#X obj 604 202 list fromsymbol;\n"
        "#X obj 604 242 list length;\n"
        "#X obj 604 282 > 0;\n"
        "#X obj 604 322 sel 1;\n"
        "#X obj 604 362 s otherware_file_path_ok;\n"
        "#X obj 48 322 r otherware_file_path_ok;\n"
        "#X msg 48 362 0 \\, 0.17 5 \\, 0 170 5;\n"
        "#X obj 48 402 vline~;\n"
        "#X obj 210 362 osc~ 805;\n"
        "#X obj 210 402 *~;\n"
        "#X obj 210 432 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 6 0;\n"
        "#X connect 1 2 11 0;\n"
        "#X connect 1 3 18 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 18 0 19 0;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 21 0 22 0;\n"
        "#X connect 22 0 23 0;\n"
        "#X connect 23 0 24 0;\n"
        "#X connect 25 0 26 0;\n"
        "#X connect 26 0 27 0;\n"
        "#X connect 27 0 29 1;\n"
        "#X connect 28 0 29 0;\n"
        "#X connect 29 0 30 0;\n"
        "#X connect 29 0 30 1;\n";

    ScDiscAudioEngine filePathEngine;
    filePathEngine.prepare (44100.0, 512, 2);
    filePathEngine.trigger (pdFilePathTrigger);

    juce::AudioBuffer<float> filePathBuffer (2, 512);
    auto filePathPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        filePathEngine.render (filePathBuffer);

        for (int channel = 0; channel < filePathBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = filePathBuffer.getReadPointer (channel);

            for (int i = 0; i < filePathBuffer.getNumSamples(); ++i)
                filePathPeak = std::max (filePathPeak, std::abs (samples[i]));
        }
    }

    filePathEngine.release();

    if (filePathPeak <= 0.0005f)
    {
        std::cerr << "Pd file path utility fixture produced no audio; peak="
                  << filePathPeak << '\n';
        return 91;
    }

    std::cout << "filePathPeak=" << filePathPeak << '\n';

    const auto fileProbePath = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                   .getChildFile ("otherware_pd_file_probe.txt");
    fileProbePath.replaceWithText ("otherware pd file probe\n");

    if (! fileProbePath.existsAsFile())
    {
        std::cerr << "Could not create Pd file probe fixture at "
                  << fileProbePath.getFullPathName() << '\n';
        return 94;
    }

    DiscAudioTrigger pdFileProbeTrigger;
    pdFileProbeTrigger.depth = 1;
    pdFileProbeTrigger.branchIndex = 44;
    pdFileProbeTrigger.pdDurationSeconds = 0.3f;
    pdFileProbeTrigger.pdPatch =
        juce::String ("#N canvas 120 120 820 500 10;\n")
        + "#X obj 48 42 r trigger;\n"
        + "#X obj 48 82 t b b b b;\n"
        + "#X msg 48 122 symbol " + fileProbePath.getFullPathName() + ";\n"
        + "#X obj 48 162 file isfile;\n"
        + "#X obj 48 202 sel 1;\n"
        + "#X obj 48 242 s otherware_file_probe_ok;\n"
        + "#X msg 224 122 symbol " + juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName() + ";\n"
        + "#X obj 224 162 file isdirectory;\n"
        + "#X obj 224 202 sel 1;\n"
        + "#X obj 224 242 s otherware_file_probe_ok;\n"
        + "#X msg 430 122 symbol " + fileProbePath.getFullPathName() + ";\n"
        + "#X obj 430 162 file size;\n"
        + "#X obj 430 202 > 0;\n"
        + "#X obj 430 242 sel 1;\n"
        + "#X obj 430 282 s otherware_file_probe_ok;\n"
        + "#X msg 632 122 symbol " + fileProbePath.getFullPathName() + ";\n"
        + "#X obj 632 162 file splitname;\n"
        + "#X obj 632 202 list length;\n"
        + "#X obj 632 242 sel 2;\n"
        + "#X obj 632 282 s otherware_file_probe_ok;\n"
        + "#X obj 48 322 r otherware_file_probe_ok;\n"
        + "#X msg 48 362 0 \\, 0.16 5 \\, 0 170 5;\n"
        + "#X obj 48 402 vline~;\n"
        + "#X obj 224 362 osc~ 765;\n"
        + "#X obj 224 402 *~;\n"
        + "#X obj 224 442 dac~;\n"
        + "#X connect 0 0 1 0;\n"
        + "#X connect 1 0 2 0;\n"
        + "#X connect 1 1 6 0;\n"
        + "#X connect 1 2 10 0;\n"
        + "#X connect 1 3 15 0;\n"
        + "#X connect 2 0 3 0;\n"
        + "#X connect 3 0 4 0;\n"
        + "#X connect 4 0 5 0;\n"
        + "#X connect 6 0 7 0;\n"
        + "#X connect 7 0 8 0;\n"
        + "#X connect 8 0 9 0;\n"
        + "#X connect 10 0 11 0;\n"
        + "#X connect 11 0 12 0;\n"
        + "#X connect 12 0 13 0;\n"
        + "#X connect 13 0 14 0;\n"
        + "#X connect 15 0 16 0;\n"
        + "#X connect 16 0 17 0;\n"
        + "#X connect 17 0 18 0;\n"
        + "#X connect 18 0 19 0;\n"
        + "#X connect 20 0 21 0;\n"
        + "#X connect 21 0 22 0;\n"
        + "#X connect 22 0 24 1;\n"
        + "#X connect 23 0 24 0;\n"
        + "#X connect 24 0 25 0;\n"
        + "#X connect 24 0 25 1;\n";

    ScDiscAudioEngine fileProbeEngine;
    fileProbeEngine.prepare (44100.0, 512, 2);
    fileProbeEngine.trigger (pdFileProbeTrigger);

    juce::AudioBuffer<float> fileProbeBuffer (2, 512);
    auto fileProbePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        fileProbeEngine.render (fileProbeBuffer);

        for (int channel = 0; channel < fileProbeBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = fileProbeBuffer.getReadPointer (channel);

            for (int i = 0; i < fileProbeBuffer.getNumSamples(); ++i)
                fileProbePeak = std::max (fileProbePeak, std::abs (samples[i]));
        }
    }

    fileProbeEngine.release();

    if (fileProbePeak <= 0.0005f)
    {
        std::cerr << "Pd file probe fixture produced no audio; peak="
                  << fileProbePeak << '\n';
        return 95;
    }

    std::cout << "fileProbePeak=" << fileProbePeak << '\n';

    DiscAudioTrigger pdFileMetadataTrigger;
    pdFileMetadataTrigger.depth = 1;
    pdFileMetadataTrigger.branchIndex = 45;
    pdFileMetadataTrigger.pdDurationSeconds = 0.3f;
    pdFileMetadataTrigger.pdPatch =
        juce::String ("#N canvas 120 120 760 460 10;\n")
        + "#X obj 48 42 r trigger;\n"
        + "#X obj 48 82 t b b;\n"
        + "#X msg 48 122 symbol " + fileProbePath.getFullPathName() + ";\n"
        + "#X obj 48 162 file stat;\n"
        + "#X obj 48 202 route type isfile size;\n"
        + "#X obj 48 242 route file;\n"
        + "#X obj 168 242 sel 1;\n"
        + "#X obj 288 242 > 0;\n"
        + "#X obj 288 282 sel 1;\n"
        + "#X obj 48 322 s otherware_file_metadata_ok;\n"
        + "#X obj 168 322 s otherware_file_metadata_ok;\n"
        + "#X obj 288 322 s otherware_file_metadata_ok;\n"
        + "#X obj 516 122 file cwd;\n"
        + "#X obj 516 162 list fromsymbol;\n"
        + "#X obj 516 202 list length;\n"
        + "#X obj 516 242 > 0;\n"
        + "#X obj 516 282 sel 1;\n"
        + "#X obj 516 322 s otherware_file_metadata_ok;\n"
        + "#X obj 48 362 r otherware_file_metadata_ok;\n"
        + "#X msg 48 402 0 \\, 0.16 5 \\, 0 170 5;\n"
        + "#X obj 48 442 vline~;\n"
        + "#X obj 224 402 osc~ 815;\n"
        + "#X obj 224 442 *~;\n"
        + "#X obj 224 482 dac~;\n"
        + "#X connect 0 0 1 0;\n"
        + "#X connect 1 0 2 0;\n"
        + "#X connect 1 1 12 0;\n"
        + "#X connect 2 0 3 0;\n"
        + "#X connect 3 0 4 0;\n"
        + "#X connect 4 0 5 0;\n"
        + "#X connect 4 1 6 0;\n"
        + "#X connect 4 2 7 0;\n"
        + "#X connect 5 0 9 0;\n"
        + "#X connect 6 0 10 0;\n"
        + "#X connect 7 0 8 0;\n"
        + "#X connect 8 0 11 0;\n"
        + "#X connect 12 0 13 0;\n"
        + "#X connect 13 0 14 0;\n"
        + "#X connect 14 0 15 0;\n"
        + "#X connect 15 0 16 0;\n"
        + "#X connect 16 0 17 0;\n"
        + "#X connect 18 0 19 0;\n"
        + "#X connect 19 0 20 0;\n"
        + "#X connect 20 0 22 1;\n"
        + "#X connect 21 0 22 0;\n"
        + "#X connect 22 0 23 0;\n"
        + "#X connect 22 0 23 1;\n";

    ScDiscAudioEngine fileMetadataEngine;
    fileMetadataEngine.prepare (44100.0, 512, 2);
    fileMetadataEngine.trigger (pdFileMetadataTrigger);

    juce::AudioBuffer<float> fileMetadataBuffer (2, 512);
    auto fileMetadataPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        fileMetadataEngine.render (fileMetadataBuffer);

        for (int channel = 0; channel < fileMetadataBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = fileMetadataBuffer.getReadPointer (channel);

            for (int i = 0; i < fileMetadataBuffer.getNumSamples(); ++i)
                fileMetadataPeak = std::max (fileMetadataPeak, std::abs (samples[i]));
        }
    }

    fileMetadataEngine.release();
    fileProbePath.deleteFile();

    if (fileMetadataPeak <= 0.0005f)
    {
        std::cerr << "Pd file metadata fixture produced no audio; peak="
                  << fileMetadataPeak << '\n';
        return 96;
    }

    std::cout << "fileMetadataPeak=" << fileMetadataPeak << '\n';

    const auto fileMutatorDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                          .getChildFile ("otherware_pd_file_mutators");
    fileMutatorDirectory.deleteRecursively();
    fileMutatorDirectory.createDirectory();

    const auto fileMutatorSource = fileMutatorDirectory.getChildFile ("source.txt");
    const auto fileMutatorCopy = fileMutatorDirectory.getChildFile ("copy.txt");
    const auto fileMutatorMoved = fileMutatorDirectory.getChildFile ("moved.txt");
    const auto fileMutatorCreatedDirectory = fileMutatorDirectory.getChildFile ("created");

    fileMutatorSource.replaceWithText ("otherware pd file mutator probe\n");

    if (! fileMutatorSource.existsAsFile())
    {
        std::cerr << "Could not create Pd file mutator source fixture at "
                  << fileMutatorSource.getFullPathName() << '\n';
        return 100;
    }

    DiscAudioTrigger pdFileMutatorTrigger;
    pdFileMutatorTrigger.depth = 1;
    pdFileMutatorTrigger.branchIndex = 47;
    pdFileMutatorTrigger.pdDurationSeconds = 0.35f;
    pdFileMutatorTrigger.pdPatch =
        juce::String ("#N canvas 120 120 980 620 10;\n")
        + "#X obj 48 42 r trigger;\n"
        + "#X obj 48 82 t b b b b b;\n"
        + "#X msg 48 122 symbol " + fileMutatorCreatedDirectory.getFullPathName() + ";\n"
        + "#X obj 48 162 file mkdir;\n"
        + "#X obj 48 202 t b;\n"
        + "#X obj 48 242 s otherware_file_mutator_ok;\n"
        + "#X msg 230 122 list " + fileMutatorSource.getFullPathName() + " " + fileMutatorCopy.getFullPathName() + ";\n"
        + "#X obj 230 162 file copy;\n"
        + "#X obj 230 202 t b b;\n"
        + "#X obj 230 242 s otherware_file_mutator_ok;\n"
        + "#X msg 388 242 list " + fileMutatorCopy.getFullPathName() + " " + fileMutatorMoved.getFullPathName() + ";\n"
        + "#X obj 388 282 file move;\n"
        + "#X obj 388 322 t b b;\n"
        + "#X obj 388 362 s otherware_file_mutator_ok;\n"
        + "#X msg 544 362 symbol " + fileMutatorMoved.getFullPathName() + ";\n"
        + "#X obj 544 402 file delete;\n"
        + "#X obj 544 442 t b;\n"
        + "#X obj 544 482 s otherware_file_mutator_ok;\n"
        + "#X msg 590 122 symbol " + fileMutatorDirectory.getFullPathName() + "/*.txt;\n"
        + "#X obj 590 162 file glob;\n"
        + "#X obj 590 202 list length;\n"
        + "#X obj 590 242 sel 2;\n"
        + "#X obj 590 282 s otherware_file_mutator_ok;\n"
        + "#X msg 750 122 symbol " + fileMutatorSource.getFullPathName() + ";\n"
        + "#X obj 750 162 file which;\n"
        + "#X obj 750 202 list length;\n"
        + "#X obj 750 242 sel 2;\n"
        + "#X obj 750 282 s otherware_file_mutator_ok;\n"
        + "#X obj 750 342 file patchpath;\n"
        + "#X obj 750 382 list fromsymbol;\n"
        + "#X obj 750 422 list length;\n"
        + "#X obj 750 462 > 0;\n"
        + "#X obj 750 502 sel 1;\n"
        + "#X obj 750 542 s otherware_file_mutator_ok;\n"
        + "#X obj 48 322 r otherware_file_mutator_ok;\n"
        + "#X obj 48 362 f 0;\n"
        + "#X obj 108 362 + 1;\n"
        + "#X obj 48 402 t f f;\n"
        + "#X obj 48 442 sel 7;\n"
        + "#X msg 48 482 0 \\, 0.17 5 \\, 0 180 5;\n"
        + "#X obj 48 522 vline~;\n"
        + "#X obj 230 482 osc~ 875;\n"
        + "#X obj 230 522 *~;\n"
        + "#X obj 230 562 dac~;\n"
        + "#X connect 0 0 1 0;\n"
        + "#X connect 1 0 2 0;\n"
        + "#X connect 1 1 6 0;\n"
        + "#X connect 1 2 18 0;\n"
        + "#X connect 1 3 23 0;\n"
        + "#X connect 1 4 28 0;\n"
        + "#X connect 2 0 3 0;\n"
        + "#X connect 3 0 4 0;\n"
        + "#X connect 4 0 5 0;\n"
        + "#X connect 6 0 7 0;\n"
        + "#X connect 7 0 8 0;\n"
        + "#X connect 8 0 9 0;\n"
        + "#X connect 8 1 10 0;\n"
        + "#X connect 10 0 11 0;\n"
        + "#X connect 11 0 12 0;\n"
        + "#X connect 12 0 13 0;\n"
        + "#X connect 12 1 14 0;\n"
        + "#X connect 14 0 15 0;\n"
        + "#X connect 15 0 16 0;\n"
        + "#X connect 16 0 17 0;\n"
        + "#X connect 18 0 19 0;\n"
        + "#X connect 19 0 20 0;\n"
        + "#X connect 20 0 21 0;\n"
        + "#X connect 21 0 22 0;\n"
        + "#X connect 23 0 24 0;\n"
        + "#X connect 24 0 25 0;\n"
        + "#X connect 25 0 26 0;\n"
        + "#X connect 26 0 27 0;\n"
        + "#X connect 28 0 29 0;\n"
        + "#X connect 29 0 30 0;\n"
        + "#X connect 30 0 31 0;\n"
        + "#X connect 31 0 32 0;\n"
        + "#X connect 32 0 33 0;\n"
        + "#X connect 34 0 35 0;\n"
        + "#X connect 35 0 36 0;\n"
        + "#X connect 36 0 37 0;\n"
        + "#X connect 37 0 38 0;\n"
        + "#X connect 37 1 35 1;\n"
        + "#X connect 38 0 39 0;\n"
        + "#X connect 39 0 40 0;\n"
        + "#X connect 40 0 42 1;\n"
        + "#X connect 41 0 42 0;\n"
        + "#X connect 42 0 43 0;\n"
        + "#X connect 42 0 43 1;\n";

    ScDiscAudioEngine fileMutatorEngine;
    fileMutatorEngine.prepare (44100.0, 512, 2);
    fileMutatorEngine.trigger (pdFileMutatorTrigger);

    juce::AudioBuffer<float> fileMutatorBuffer (2, 512);
    auto fileMutatorPeak = 0.0f;

    for (int block = 0; block < 100; ++block)
    {
        fileMutatorEngine.render (fileMutatorBuffer);

        for (int channel = 0; channel < fileMutatorBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = fileMutatorBuffer.getReadPointer (channel);

            for (int i = 0; i < fileMutatorBuffer.getNumSamples(); ++i)
                fileMutatorPeak = std::max (fileMutatorPeak, std::abs (samples[i]));
        }
    }

    fileMutatorEngine.release();

    const auto mutatorSideEffectsOk = fileMutatorSource.existsAsFile()
                                   && fileMutatorCreatedDirectory.isDirectory()
                                   && ! fileMutatorCopy.exists()
                                   && ! fileMutatorMoved.exists();
    fileMutatorDirectory.deleteRecursively();

    if (fileMutatorPeak <= 0.0005f || ! mutatorSideEffectsOk)
    {
        std::cerr << "Pd file mutator/search fixture failed; peak="
                  << fileMutatorPeak
                  << " sideEffectsOk=" << mutatorSideEffectsOk << '\n';
        return 101;
    }

    std::cout << "fileMutatorPeak=" << fileMutatorPeak << '\n';

    const auto fileHandleDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                         .getChildFile ("otherware_pd_file_handle");
    fileHandleDirectory.deleteRecursively();
    fileHandleDirectory.createDirectory();

    const auto fileHandlePath = fileHandleDirectory.getChildFile ("bytes.bin");

    DiscAudioTrigger pdFileHandleTrigger;
    pdFileHandleTrigger.depth = 1;
    pdFileHandleTrigger.branchIndex = 48;
    pdFileHandleTrigger.pdDurationSeconds = 0.35f;
    pdFileHandleTrigger.pdPatch =
        juce::String ("#N canvas 120 120 760 500 10;\n")
        + "#X obj 48 42 r trigger;\n"
        + "#X obj 48 82 t b b;\n"
        + "#X obj 48 122 t b b b;\n"
        + "#X msg 48 162 close;\n"
        + "#X msg 146 162 list 65 66 67;\n"
        + "#X msg 286 162 open " + fileHandlePath.getFullPathName() + " c;\n"
        + "#X obj 146 222 file handle;\n"
        + "#X obj 430 122 del 30;\n"
        + "#X obj 430 162 t b b b;\n"
        + "#X msg 430 202 list 3;\n"
        + "#X msg 528 202 seek 0;\n"
        + "#X msg 626 202 open " + fileHandlePath.getFullPathName() + " r;\n"
        + "#X obj 528 262 file handle;\n"
        + "#X obj 528 302 list length;\n"
        + "#X obj 528 342 sel 3;\n"
        + "#X obj 528 382 s otherware_file_handle_ok;\n"
        + "#X obj 48 302 r otherware_file_handle_ok;\n"
        + "#X msg 48 342 0 \\, 0.17 5 \\, 0 180 5;\n"
        + "#X obj 48 382 vline~;\n"
        + "#X obj 220 342 osc~ 935;\n"
        + "#X obj 220 382 *~;\n"
        + "#X obj 220 422 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 7 0;\n"
        "#X connect 1 1 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 2 1 4 0;\n"
        "#X connect 2 2 5 0;\n"
        "#X connect 3 0 6 0;\n"
        "#X connect 4 0 6 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 1 10 0;\n"
        "#X connect 8 2 11 0;\n"
        "#X connect 9 0 12 0;\n"
        "#X connect 10 0 12 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 18 0 20 1;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 20 0 21 1;\n";

    ScDiscAudioEngine fileHandleEngine;
    fileHandleEngine.prepare (44100.0, 512, 2);
    fileHandleEngine.trigger (pdFileHandleTrigger);

    juce::AudioBuffer<float> fileHandleBuffer (2, 512);
    auto fileHandlePeak = 0.0f;

    for (int block = 0; block < 100; ++block)
    {
        fileHandleEngine.render (fileHandleBuffer);

        for (int channel = 0; channel < fileHandleBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = fileHandleBuffer.getReadPointer (channel);

            for (int i = 0; i < fileHandleBuffer.getNumSamples(); ++i)
                fileHandlePeak = std::max (fileHandlePeak, std::abs (samples[i]));
        }
    }

    fileHandleEngine.release();

    juce::MemoryBlock fileHandleBytes;
    fileHandlePath.loadFileAsData (fileHandleBytes);
    const auto* fileHandleData = static_cast<const unsigned char*> (fileHandleBytes.getData());
    const auto fileHandleSideEffectsOk = fileHandleBytes.getSize() == 3
                                      && fileHandleData[0] == 65
                                      && fileHandleData[1] == 66
                                      && fileHandleData[2] == 67;
    fileHandleDirectory.deleteRecursively();

    if (fileHandlePeak <= 0.0005f || ! fileHandleSideEffectsOk)
    {
        std::cerr << "Pd file handle fixture failed; peak="
                  << fileHandlePeak
                  << " sideEffectsOk=" << fileHandleSideEffectsOk << '\n';
        return 102;
    }

    std::cout << "fileHandlePeak=" << fileHandlePeak << '\n';

    const auto sharedHandleDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                           .getChildFile ("otherware_pd_shared_file_handle");
    sharedHandleDirectory.deleteRecursively();
    sharedHandleDirectory.createDirectory();

    const auto sharedHandlePath = sharedHandleDirectory.getChildFile ("shared-bytes.bin");

    DiscAudioTrigger pdSharedFileHandleTrigger;
    pdSharedFileHandleTrigger.depth = 1;
    pdSharedFileHandleTrigger.branchIndex = 49;
    pdSharedFileHandleTrigger.pdDurationSeconds = 0.35f;
    pdSharedFileHandleTrigger.pdPatch =
        juce::String ("#N canvas 120 120 780 520 10;\n")
        + "#X obj 48 42 r trigger;\n"
        + "#X obj 48 82 t b b;\n"
        + "#X obj 48 122 t b b b;\n"
        + "#X msg 48 162 close;\n"
        + "#X msg 146 162 list 68 69 70 71;\n"
        + "#X msg 308 162 open " + sharedHandlePath.getFullPathName() + " c;\n"
        + "#X obj 146 222 file handle otherware_shared_handle;\n"
        + "#X obj 438 122 del 35;\n"
        + "#X obj 438 162 t b b b;\n"
        + "#X msg 438 202 list 4;\n"
        + "#X msg 536 202 seek 0;\n"
        + "#X msg 634 202 open " + sharedHandlePath.getFullPathName() + " r;\n"
        + "#X obj 536 262 file handle otherware_shared_handle;\n"
        + "#X obj 536 302 list length;\n"
        + "#X obj 536 342 sel 4;\n"
        + "#X obj 536 382 s otherware_shared_file_handle_ok;\n"
        + "#X obj 48 302 r otherware_shared_file_handle_ok;\n"
        + "#X msg 48 342 0 \\, 0.17 5 \\, 0 180 5;\n"
        + "#X obj 48 382 vline~;\n"
        + "#X obj 220 342 osc~ 965;\n"
        + "#X obj 220 382 *~;\n"
        + "#X obj 220 422 dac~;\n"
        + "#X obj 308 302 file define otherware_shared_handle;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 7 0;\n"
        "#X connect 1 1 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 2 1 4 0;\n"
        "#X connect 2 2 5 0;\n"
        "#X connect 3 0 6 0;\n"
        "#X connect 4 0 6 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 1 10 0;\n"
        "#X connect 8 2 11 0;\n"
        "#X connect 9 0 12 0;\n"
        "#X connect 10 0 12 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 18 0 20 1;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 20 0 21 1;\n";

    ScDiscAudioEngine sharedFileHandleEngine;
    sharedFileHandleEngine.prepare (44100.0, 512, 2);
    sharedFileHandleEngine.trigger (pdSharedFileHandleTrigger);

    juce::AudioBuffer<float> sharedFileHandleBuffer (2, 512);
    auto sharedFileHandlePeak = 0.0f;

    for (int block = 0; block < 100; ++block)
    {
        sharedFileHandleEngine.render (sharedFileHandleBuffer);

        for (int channel = 0; channel < sharedFileHandleBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = sharedFileHandleBuffer.getReadPointer (channel);

            for (int i = 0; i < sharedFileHandleBuffer.getNumSamples(); ++i)
                sharedFileHandlePeak = std::max (sharedFileHandlePeak, std::abs (samples[i]));
        }
    }

    sharedFileHandleEngine.release();

    juce::MemoryBlock sharedHandleBytes;
    sharedHandlePath.loadFileAsData (sharedHandleBytes);
    const auto* sharedHandleData = static_cast<const unsigned char*> (sharedHandleBytes.getData());
    const auto sharedHandleSideEffectsOk = sharedHandleBytes.getSize() == 4
                                        && sharedHandleData[0] == 68
                                        && sharedHandleData[1] == 69
                                        && sharedHandleData[2] == 70
                                        && sharedHandleData[3] == 71;
    sharedHandleDirectory.deleteRecursively();

    if (sharedFileHandlePeak <= 0.0005f || ! sharedHandleSideEffectsOk)
    {
        std::cerr << "Pd shared file define/handle fixture failed; peak="
                  << sharedFileHandlePeak
                  << " sideEffectsOk=" << sharedHandleSideEffectsOk << '\n';
        return 103;
    }

    std::cout << "sharedFileHandlePeak=" << sharedFileHandlePeak << '\n';

    DiscAudioTrigger pdNetworkTrigger;
    pdNetworkTrigger.depth = 1;
    pdNetworkTrigger.branchIndex = 42;
    pdNetworkTrigger.pdDurationSeconds = 0.35f;
    pdNetworkTrigger.pdPatch =
        "#N canvas 120 120 720 420 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b;\n"
        "#X obj 48 122 del 20;\n"
        "#X msg 48 162 connect localhost 58391;\n"
        "#X obj 48 242 netsend -u;\n"
        "#X obj 48 282 sel 1;\n"
        "#X obj 48 322 del 20;\n"
        "#X msg 48 362 send 84;\n"
        "#X obj 304 82 netreceive -u 58391;\n"
        "#X obj 304 122 mtof;\n"
        "#X obj 304 162 osc~;\n"
        "#X msg 424 122 0 \\, 0.18 5 \\, 0 190 5;\n"
        "#X obj 424 162 vline~;\n"
        "#X obj 304 202 *~;\n"
        "#X obj 304 242 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 4 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 11 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 13 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 12 0 13 1;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 13 0 14 1;\n";

    ScDiscAudioEngine networkEngine;
    networkEngine.prepare (44100.0, 512, 2);
    networkEngine.trigger (pdNetworkTrigger);

    juce::AudioBuffer<float> networkBuffer (2, 512);
    auto networkPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        networkEngine.render (networkBuffer);

        for (int channel = 0; channel < networkBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = networkBuffer.getReadPointer (channel);

            for (int i = 0; i < networkBuffer.getNumSamples(); ++i)
                networkPeak = std::max (networkPeak, std::abs (samples[i]));
        }
    }

    networkEngine.release();

    if (networkPeak <= 0.0005f)
    {
        std::cerr << "Pd netsend/netreceive UDP fixture produced no audio; peak="
                  << networkPeak << '\n';
        return 92;
    }

    std::cout << "networkPeak=" << networkPeak << '\n';

    DiscAudioTrigger pdTcpNetworkTrigger;
    pdTcpNetworkTrigger.depth = 1;
    pdTcpNetworkTrigger.branchIndex = 52;
    pdTcpNetworkTrigger.pdDurationSeconds = 0.35f;
    pdTcpNetworkTrigger.pdPatch =
        "#N canvas 120 120 780 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b;\n"
        "#X obj 48 122 del 25;\n"
        "#X msg 48 162 connect localhost 58421;\n"
        "#X obj 48 242 netsend;\n"
        "#X obj 48 282 sel 1;\n"
        "#X obj 48 322 del 25;\n"
        "#X msg 48 362 send 86;\n"
        "#X obj 330 82 netreceive 58421;\n"
        "#X obj 330 122 t f f;\n"
        "#X obj 330 162 mtof;\n"
        "#X obj 330 202 osc~;\n"
        "#X msg 470 162 0 \\, 0.18 5 \\, 0 190 5;\n"
        "#X obj 470 202 vline~;\n"
        "#X obj 330 242 *~;\n"
        "#X obj 330 282 dac~;\n"
        "#X obj 560 122 > 0;\n"
        "#X obj 560 162 sel 1;\n"
        "#X obj 560 202 s otherware_tcp_connection_seen;\n"
        "#X obj 470 122 r otherware_tcp_connection_seen;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 4 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 1 16 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 1 12 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 14 0;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 1;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 14 0 15 1;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 19 0 12 0;\n";

    ScDiscAudioEngine tcpNetworkEngine;
    tcpNetworkEngine.prepare (44100.0, 512, 2);
    tcpNetworkEngine.trigger (pdTcpNetworkTrigger);

    juce::AudioBuffer<float> tcpNetworkBuffer (2, 512);
    auto tcpNetworkPeak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        tcpNetworkEngine.render (tcpNetworkBuffer);

        for (int channel = 0; channel < tcpNetworkBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = tcpNetworkBuffer.getReadPointer (channel);

            for (int i = 0; i < tcpNetworkBuffer.getNumSamples(); ++i)
                tcpNetworkPeak = std::max (tcpNetworkPeak, std::abs (samples[i]));
        }
    }

    tcpNetworkEngine.release();

    if (tcpNetworkPeak <= 0.0005f)
    {
        std::cerr << "Pd TCP netsend/netreceive fixture produced no audio; peak="
                  << tcpNetworkPeak << '\n';
        return 106;
    }

    std::cout << "tcpNetworkPeak=" << tcpNetworkPeak << '\n';

    DiscAudioTrigger pdControlTrigger;
    pdControlTrigger.depth = 1;
    pdControlTrigger.branchIndex = 43;
    pdControlTrigger.pdDurationSeconds = 0.3f;
    pdControlTrigger.pdPatch =
        "#N canvas 120 120 720 460 10;\n"
        "#X obj 48 42 r trigger;\n"
        "#X obj 48 82 t b b;\n"
        "#X msg 48 122 dir 0;\n"
        "#X obj 48 162 pdcontrol;\n"
        "#X obj 48 202 list fromsymbol;\n"
        "#X obj 48 242 list length;\n"
        "#X obj 48 282 > 0;\n"
        "#X obj 48 322 sel 1;\n"
        "#X obj 48 362 s otherware_pdcontrol_ok;\n"
        "#X msg 280 122 isvisible;\n"
        "#X obj 280 162 pdcontrol;\n"
        "#X obj 280 202 sel 0;\n"
        "#X obj 280 242 s otherware_pdcontrol_ok;\n"
        "#X obj 484 122 r otherware_pdcontrol_ok;\n"
        "#X msg 484 162 0 \\, 0.16 5 \\, 0 170 5;\n"
        "#X obj 484 202 vline~;\n"
        "#X obj 584 162 osc~ 715;\n"
        "#X obj 584 202 *~;\n"
        "#X obj 584 242 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 9 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 15 0 17 1;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 17 0 18 1;\n";

    ScDiscAudioEngine pdControlEngine;
    pdControlEngine.prepare (44100.0, 512, 2);
    pdControlEngine.trigger (pdControlTrigger);

    juce::AudioBuffer<float> pdControlBuffer (2, 512);
    auto pdControlPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        pdControlEngine.render (pdControlBuffer);

        for (int channel = 0; channel < pdControlBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = pdControlBuffer.getReadPointer (channel);

            for (int i = 0; i < pdControlBuffer.getNumSamples(); ++i)
                pdControlPeak = std::max (pdControlPeak, std::abs (samples[i]));
        }
    }

    pdControlEngine.release();

    if (pdControlPeak <= 0.0005f)
    {
        std::cerr << "Pd pdcontrol fixture produced no audio; peak="
                  << pdControlPeak << '\n';
        return 93;
    }

    std::cout << "pdcontrolPeak=" << pdControlPeak << '\n';

    DiscAudioTrigger pdMidiInputTrigger;
    pdMidiInputTrigger.depth = 1;
    pdMidiInputTrigger.branchIndex = 46;
    pdMidiInputTrigger.pdDurationSeconds = 0.35f;
    pdMidiInputTrigger.pdPatch =
        "#N canvas 120 120 900 620 10;\n"
        "#X obj 48 42 notein 1;\n"
        "#X obj 48 82 sel 72;\n"
        "#X obj 48 122 s otherware_midi_input_ok;\n"
        "#X obj 180 42 ctlin 7 1;\n"
        "#X obj 180 82 sel 101;\n"
        "#X obj 180 122 s otherware_midi_input_ok;\n"
        "#X obj 320 42 pgmin 1;\n"
        "#X obj 320 82 sel 13;\n"
        "#X obj 320 122 s otherware_midi_input_ok;\n"
        "#X obj 456 42 bendin 1;\n"
        "#X obj 456 82 > 8192;\n"
        "#X obj 456 122 sel 1;\n"
        "#X obj 456 162 s otherware_midi_input_ok;\n"
        "#X obj 604 42 touchin 1;\n"
        "#X obj 604 82 sel 74;\n"
        "#X obj 604 122 s otherware_midi_input_ok;\n"
        "#X obj 48 222 polytouchin 1;\n"
        "#X obj 48 262 sel 88;\n"
        "#X obj 48 302 s otherware_midi_input_ok;\n"
        "#X obj 220 222 midirealtimein;\n"
        "#X obj 220 262 sel 248;\n"
        "#X obj 220 302 s otherware_midi_input_ok;\n"
        "#X obj 420 222 sysexin;\n"
        "#X obj 420 262 sel 125;\n"
        "#X obj 420 302 s otherware_midi_input_ok;\n"
        "#X obj 604 222 r otherware_midi_input_ok;\n"
        "#X obj 604 262 f 0;\n"
        "#X obj 664 262 + 1;\n"
        "#X obj 604 302 t f f;\n"
        "#X obj 604 342 sel 9;\n"
        "#X msg 604 382 0 \\, 0.18 5 \\, 0 190 5;\n"
        "#X obj 604 422 vline~;\n"
        "#X obj 744 382 osc~ 925;\n"
        "#X obj 744 422 *~;\n"
        "#X obj 744 462 dac~;\n"
        "#X obj 48 502 notein 17;\n"
        "#X obj 48 542 sel 73;\n"
        "#X obj 48 582 s otherware_midi_input_ok;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 15 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 17 0 18 0;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 22 0 23 0;\n"
        "#X connect 23 0 24 0;\n"
        "#X connect 25 0 26 0;\n"
        "#X connect 26 0 27 0;\n"
        "#X connect 27 0 28 0;\n"
        "#X connect 28 0 29 0;\n"
        "#X connect 28 1 26 1;\n"
        "#X connect 29 0 30 0;\n"
        "#X connect 30 0 31 0;\n"
        "#X connect 31 0 33 1;\n"
        "#X connect 32 0 33 0;\n"
        "#X connect 33 0 34 0;\n"
        "#X connect 33 0 34 1;\n"
        "#X connect 35 0 36 0;\n"
        "#X connect 36 0 37 0;\n";

    ScDiscAudioEngine midiInputEngine;
    midiInputEngine.prepare (44100.0, 512, 2);
    midiInputEngine.trigger (pdMidiInputTrigger);

    juce::AudioBuffer<float> midiInputBuffer (2, 512);
    auto midiInputPeak = 0.0f;

    for (int block = 0; block < 4; ++block)
        midiInputEngine.render (midiInputBuffer);

    const unsigned char midiSysExByte = 0x7d;
    const auto midiMessagesSent =
        midiInputEngine.sendPdMidiMessage (juce::MidiMessage::noteOn (1, 72, static_cast<juce::uint8> (96)))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::controllerEvent (1, 7, 101))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::programChange (1, 12))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::pitchWheel (1, 10240))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::channelPressureChange (1, 74))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::aftertouchChange (1, 65, 88))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::midiClock())
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::createSysExMessage (&midiSysExByte, 1))
        && midiInputEngine.sendPdMidiMessage (juce::MidiMessage::noteOn (1, 73, static_cast<juce::uint8> (96)), 1);

    if (! midiMessagesSent)
    {
        std::cerr << "JUCE MIDI messages failed to route into Pd\n";
        return 97;
    }

    for (int block = 0; block < 100; ++block)
    {
        midiInputEngine.render (midiInputBuffer);

        for (int channel = 0; channel < midiInputBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = midiInputBuffer.getReadPointer (channel);

            for (int i = 0; i < midiInputBuffer.getNumSamples(); ++i)
                midiInputPeak = std::max (midiInputPeak, std::abs (samples[i]));
        }
    }

    midiInputEngine.release();

    if (midiInputPeak <= 0.0005f)
    {
        std::cerr << "Pd MIDI input fixture produced no audio; peak="
                  << midiInputPeak << '\n';
        return 98;
    }

    std::cout << "midiInputPeak=" << midiInputPeak << '\n';

    ScDiscAudioEngine midiOutputEngine;
    midiOutputEngine.prepare (44100.0, 512, 2);
    std::vector<std::pair<int, juce::MidiMessage>> routedMidiOutput;
    midiOutputEngine.setPdMidiOutputCallback ([&routedMidiOutput] (int port, const juce::MidiMessage& message)
    {
        routedMidiOutput.emplace_back (port, message);
    });

    if (! midiOutputEngine.runPdMidiOutputSmoke())
    {
        std::cerr << "Pd MIDI output fixture did not receive all expected hook events; "
                  << midiOutputEngine.getStatusText() << '\n';
        return 99;
    }

    const auto routed = [&routedMidiOutput] (auto predicate)
    {
        return std::any_of (routedMidiOutput.begin(), routedMidiOutput.end(), [&predicate] (const auto& routedMessage)
        {
            return predicate (routedMessage.first, routedMessage.second);
        });
    };

    const auto midiOutputTranslated =
        routed ([] (int port, const auto& m) { return port == 0 && m.isNoteOn() && m.getChannel() == 1 && m.getNoteNumber() == 72 && m.getVelocity() == 96; })
        && routed ([] (int port, const auto& m) { return port == 0 && m.isController() && m.getControllerNumber() == 7 && m.getControllerValue() == 101; })
        && routed ([] (int port, const auto& m) { return port == 0 && m.isProgramChange() && m.getProgramChangeNumber() == 12; })
        && routed ([] (int port, const auto& m) { return port == 0 && m.isPitchWheel() && m.getPitchWheelValue() == 10240; })
        && routed ([] (int port, const auto& m) { return port == 0 && m.isChannelPressure() && m.getChannelPressureValue() == 74; })
        && routed ([] (int port, const auto& m) { return port == 0 && m.isAftertouch() && m.getNoteNumber() == 65 && m.getAfterTouchValue() == 88; })
        && routed ([] (int port, const auto& m) { return port == 1 && m.isNoteOn() && m.getChannel() == 1 && m.getNoteNumber() == 72 && m.getVelocity() == 96; });

    if (! midiOutputTranslated)
    {
        std::cerr << "Pd MIDI output hooks did not translate into JUCE MIDI messages\n";
        return 108;
    }

    midiOutputEngine.release();
    std::cout << "midiOutputHooks=7/7 routed=" << routedMidiOutput.size() << '\n';

    ScDiscAudioEngine messageOutputEngine;
    messageOutputEngine.prepare (44100.0, 512, 2);
    juce::StringArray routedPdMessages;
    messageOutputEngine.setPdMessageOutputCallback ([&routedPdMessages] (const juce::String& receiver,
                                                                         const juce::String& selector,
                                                                         const juce::StringArray& atoms)
    {
        routedPdMessages.add (receiver + ":" + selector + ":" + atoms.joinIntoString (","));
    });
    messageOutputEngine.setPdMessageSubscriptions ({ "otherware_message_smoke" });
    DiscAudioTrigger pdMessageOutputTrigger;
    pdMessageOutputTrigger.pdDurationSeconds = 0.2f;
    pdMessageOutputTrigger.pdPatch =
        "#N canvas 120 120 520 360 10;\n"
        "#X obj 40 40 loadbang;\n"
        "#X obj 40 80 t b b b b;\n"
        "#X msg 40 130 12.5;\n"
        "#X msg 130 130 symbol hello;\n"
        "#X msg 250 130 1 2 three;\n"
        "#X msg 370 130 custom 4 five;\n"
        "#X obj 40 190 s otherware_message_smoke;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 3 2 0;\n"
        "#X connect 1 2 3 0;\n"
        "#X connect 1 1 4 0;\n"
        "#X connect 1 0 5 0;\n"
        "#X connect 2 0 6 0;\n"
        "#X connect 3 0 6 0;\n"
        "#X connect 4 0 6 0;\n"
        "#X connect 5 0 6 0;\n";
    messageOutputEngine.trigger (pdMessageOutputTrigger);
    const auto hasPdMessage = [&routedPdMessages] (const juce::String& expected)
    {
        return routedPdMessages.contains (expected);
    };
    if (! hasPdMessage ("otherware_message_smoke:float:12.5")
        || ! hasPdMessage ("otherware_message_smoke:symbol:hello")
        || ! hasPdMessage ("otherware_message_smoke:list:1,2,three")
        || ! hasPdMessage ("otherware_message_smoke:custom:4,five"))
    {
        std::cerr << "Pd message output hooks missed subscribed traffic: "
                  << routedPdMessages.joinIntoString (" | ") << '\n';
        return 109;
    }
    const juce::String dollarZeroOutputPatch =
        "#N canvas 120 120 420 260 10;\n"
        "#X obj 40 40 r otherware_dollarzero_start;\n"
        "#X msg 40 80 37;\n"
        "#X obj 40 120 s \\$0-otherware_state;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n";
    messageOutputEngine.triggerPdMessage (dollarZeroOutputPatch,
                                          "otherware_dollarzero_start",
                                          "bang",
                                          {},
                                          -1.0f,
                                          {});
    const auto dollarZero = messageOutputEngine.getPdPatchDollarZero (dollarZeroOutputPatch);
    const auto resolvedStateReceiver = juce::String (dollarZero) + "-otherware_state";
    messageOutputEngine.setPdMessageSubscriptions ({ resolvedStateReceiver });
    messageOutputEngine.triggerPdMessage (dollarZeroOutputPatch,
                                          "otherware_dollarzero_start",
                                          "bang",
                                          {},
                                          -1.0f,
                                          {});
    if (dollarZero <= 0 || ! hasPdMessage (resolvedStateReceiver + ":float:37"))
    {
        std::cerr << "Pd $0 output subscription failed: " << dollarZero << " / "
                  << routedPdMessages.joinIntoString (" | ") << '\n';
        return 110;
    }

    const juce::String liveArrayPatch =
        "#N canvas 120 120 420 260 10;\n"
        "#X obj 40 40 r otherware_array_set;\n"
        "#X obj 40 80 array set \\$0-otherware_live;\n"
        "#X obj 40 130 array define \\$0-otherware_live 4;\n"
        "#X connect 0 0 1 0;\n";
    if (! messageOutputEngine.triggerPdMessage (liveArrayPatch,
                                                 "otherware_array_set",
                                                 "list",
                                                 { "0.125", "-0.25", "0.5", "0.75" },
                                                 -1.0f,
                                                 {}))
    {
        std::cerr << "Pd live-array fixture did not load\n";
        return 111;
    }
    const auto liveArrayDollarZero = messageOutputEngine.getPdPatchDollarZero (liveArrayPatch);
    const auto liveArrayName = juce::String (liveArrayDollarZero) + "-otherware_live";
    const auto liveArrays = messageOutputEngine.readPdArrays (liveArrayPatch, { liveArrayName });
    if (liveArrayDollarZero <= 0 || liveArrays.size() != 1 || liveArrays.front().totalSize != 4
        || liveArrays.front().values.size() != 4
        || std::abs (liveArrays.front().values[0] - 0.125f) > 0.00001f
        || std::abs (liveArrays.front().values[1] + 0.25f) > 0.00001f
        || std::abs (liveArrays.front().values[2] - 0.5f) > 0.00001f
        || std::abs (liveArrays.front().values[3] - 0.75f) > 0.00001f)
    {
        std::cerr << "Pd live-array snapshot mismatch\n";
        return 112;
    }
    if (! messageOutputEngine.readPdArrays (liveArrayPatch, { "missing-array" }).empty())
    {
        std::cerr << "Pd live-array reader returned an unknown array\n";
        return 113;
    }
    messageOutputEngine.stopPreview();
    if (! messageOutputEngine.readPdArrays (liveArrayPatch, { liveArrayName }).empty())
    {
        std::cerr << "Pd live-array reader retained an unloaded patch\n";
        return 114;
    }
    std::cout << "pdLiveArray=4/4\n";
    messageOutputEngine.release();
    std::cout << "pdMessageOutputHooks=" << routedPdMessages.size() << '\n';

    DiscAudioTrigger pdListStoreTrigger;
    pdListStoreTrigger.depth = 1;
    pdListStoreTrigger.branchIndex = 24;
    pdListStoreTrigger.pdDurationSeconds = 0.3f;
    pdListStoreTrigger.pdPatch =
        "#N canvas 120 120 760 480 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 83;\n"
        "#X obj 48 122 list store;\n"
        "#X msg 148 82 get 0 1;\n"
        "#X obj 48 162 unpack f;\n"
        "#X obj 48 202 mtof;\n"
        "#X obj 48 242 osc~;\n"
        "#X obj 48 282 *~ 0.2;\n"
        "#X obj 48 322 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 3 0;\n"
        "#X connect 1 0 2 1;\n"
        "#X connect 3 0 2 0;\n"
        "#X connect 2 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 7 0 8 1;\n";

    ScDiscAudioEngine listStoreEngine;
    listStoreEngine.prepare (44100.0, 512, 2);
    listStoreEngine.trigger (pdListStoreTrigger);

    juce::AudioBuffer<float> listStoreBuffer (2, 512);
    auto listStorePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        listStoreEngine.render (listStoreBuffer);

        for (int channel = 0; channel < listStoreBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = listStoreBuffer.getReadPointer (channel);

            for (int i = 0; i < listStoreBuffer.getNumSamples(); ++i)
                listStorePeak = std::max (listStorePeak, std::abs (samples[i]));
        }
    }

    listStoreEngine.release();

    if (listStorePeak <= 0.0005f)
    {
        std::cerr << "Pd list store fixture produced no audio; peak="
                  << listStorePeak << '\n';
        return 29;
    }

    std::cout << "listStorePeak=" << listStorePeak << '\n';

    DiscAudioTrigger pdListSymbolBagTrigger;
    pdListSymbolBagTrigger.depth = 1;
    pdListSymbolBagTrigger.branchIndex = 50;
    pdListSymbolBagTrigger.pdDurationSeconds = 0.3f;
    pdListSymbolBagTrigger.pdPatch =
        "#N canvas 120 120 820 520 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X obj 48 82 t b b b;\n"
        "#X msg 48 122 symbol Pd;\n"
        "#X obj 48 162 list fromsymbol;\n"
        "#X obj 48 202 list length;\n"
        "#X obj 48 242 sel 2;\n"
        "#X obj 48 282 s otherware_list_symbol_bag_ok;\n"
        "#X msg 276 122 79 87;\n"
        "#X obj 276 162 list tosymbol;\n"
        "#X obj 276 202 sel OW;\n"
        "#X obj 276 242 s otherware_list_symbol_bag_ok;\n"
        "#X obj 526 122 t b b b;\n"
        "#X msg 526 162 count 64;\n"
        "#X msg 626 162 64;\n"
        "#X msg 704 162 1;\n"
        "#X obj 626 222 bag;\n"
        "#X obj 686 262 sel 1;\n"
        "#X obj 686 302 s otherware_list_symbol_bag_ok;\n"
        "#X obj 48 342 r otherware_list_symbol_bag_ok;\n"
        "#X obj 48 382 f 0;\n"
        "#X obj 108 382 + 1;\n"
        "#X obj 48 422 t f f;\n"
        "#X obj 48 462 sel 3;\n"
        "#X msg 48 502 0 \\, 0.17 5 \\, 0 180 5;\n"
        "#X obj 48 542 vline~;\n"
        "#X obj 230 502 osc~ 995;\n"
        "#X obj 230 542 *~;\n"
        "#X obj 230 582 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 7 0;\n"
        "#X connect 1 2 11 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 11 0 12 0;\n"
        "#X connect 11 1 13 0;\n"
        "#X connect 11 2 14 0;\n"
        "#X connect 12 0 15 0;\n"
        "#X connect 13 0 15 0;\n"
        "#X connect 14 0 15 1;\n"
        "#X connect 15 1 16 0;\n"
        "#X connect 16 0 17 0;\n"
        "#X connect 18 0 19 0;\n"
        "#X connect 19 0 20 0;\n"
        "#X connect 20 0 21 0;\n"
        "#X connect 21 0 22 0;\n"
        "#X connect 21 1 19 1;\n"
        "#X connect 22 0 23 0;\n"
        "#X connect 23 0 24 0;\n"
        "#X connect 24 0 26 1;\n"
        "#X connect 25 0 26 0;\n"
        "#X connect 26 0 27 0;\n"
        "#X connect 26 0 27 1;\n";

    ScDiscAudioEngine listSymbolBagEngine;
    listSymbolBagEngine.prepare (44100.0, 512, 2);
    listSymbolBagEngine.trigger (pdListSymbolBagTrigger);

    juce::AudioBuffer<float> listSymbolBagBuffer (2, 512);
    auto listSymbolBagPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        listSymbolBagEngine.render (listSymbolBagBuffer);

        for (int channel = 0; channel < listSymbolBagBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = listSymbolBagBuffer.getReadPointer (channel);

            for (int i = 0; i < listSymbolBagBuffer.getNumSamples(); ++i)
                listSymbolBagPeak = std::max (listSymbolBagPeak, std::abs (samples[i]));
        }
    }

    listSymbolBagEngine.release();

    if (listSymbolBagPeak <= 0.0005f)
    {
        std::cerr << "Pd list symbol/bag fixture produced no audio; peak="
                  << listSymbolBagPeak << '\n';
        return 104;
    }

    std::cout << "listSymbolBagPeak=" << listSymbolBagPeak << '\n';

    DiscAudioTrigger pdPolyTrigger;
    pdPolyTrigger.depth = 1;
    pdPolyTrigger.branchIndex = 25;
    pdPolyTrigger.pdDurationSeconds = 0.3f;
    pdPolyTrigger.pdPatch =
        "#N canvas 120 120 780 500 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 72;\n"
        "#X msg 112 82 96;\n"
        "#X msg 176 82 120;\n"
        "#X obj 48 122 makenote;\n"
        "#X obj 48 162 poly 4 1;\n"
        "#X obj 48 202 unpack f f f;\n"
        "#X obj 128 242 mtof;\n"
        "#X obj 128 282 osc~;\n"
        "#X obj 128 322 *~ 0.2;\n"
        "#X obj 128 362 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 2 0;\n"
        "#X connect 0 0 3 0;\n"
        "#X connect 1 0 4 0;\n"
        "#X connect 2 0 4 1;\n"
        "#X connect 3 0 4 2;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 1 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 1 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 10 1;\n";

    ScDiscAudioEngine polyEngine;
    polyEngine.prepare (44100.0, 512, 2);
    polyEngine.trigger (pdPolyTrigger);

    juce::AudioBuffer<float> polyBuffer (2, 512);
    auto polyPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        polyEngine.render (polyBuffer);

        for (int channel = 0; channel < polyBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = polyBuffer.getReadPointer (channel);

            for (int i = 0; i < polyBuffer.getNumSamples(); ++i)
                polyPeak = std::max (polyPeak, std::abs (samples[i]));
        }
    }

    polyEngine.release();

    if (polyPeak <= 0.0005f)
    {
        std::cerr << "Pd poly/makenote fixture produced no audio; peak="
                  << polyPeak << '\n';
        return 30;
    }

    std::cout << "polyPeak=" << polyPeak << '\n';

    DiscAudioTrigger pdStripnoteTrigger;
    pdStripnoteTrigger.depth = 1;
    pdStripnoteTrigger.branchIndex = 26;
    pdStripnoteTrigger.pdDurationSeconds = 0.3f;
    pdStripnoteTrigger.pdPatch =
        "#N canvas 120 120 720 460 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 60;\n"
        "#X msg 112 82 0;\n"
        "#X msg 176 82 84;\n"
        "#X msg 240 82 90;\n"
        "#X obj 48 132 stripnote;\n"
        "#X obj 48 172 mtof;\n"
        "#X obj 48 212 osc~;\n"
        "#X obj 48 252 *~ 0.2;\n"
        "#X obj 48 292 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 0 2 0;\n"
        "#X connect 0 0 3 0;\n"
        "#X connect 0 0 4 0;\n"
        "#X connect 1 0 5 0;\n"
        "#X connect 2 0 5 1;\n"
        "#X connect 3 0 5 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine stripnoteEngine;
    stripnoteEngine.prepare (44100.0, 512, 2);
    stripnoteEngine.trigger (pdStripnoteTrigger);

    juce::AudioBuffer<float> stripnoteBuffer (2, 512);
    auto stripnotePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        stripnoteEngine.render (stripnoteBuffer);

        for (int channel = 0; channel < stripnoteBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = stripnoteBuffer.getReadPointer (channel);

            for (int i = 0; i < stripnoteBuffer.getNumSamples(); ++i)
                stripnotePeak = std::max (stripnotePeak, std::abs (samples[i]));
        }
    }

    stripnoteEngine.release();

    if (stripnotePeak <= 0.0005f)
    {
        std::cerr << "Pd stripnote fixture produced no audio; peak="
                  << stripnotePeak << '\n';
        return 31;
    }

    std::cout << "stripnotePeak=" << stripnotePeak << '\n';

    DiscAudioTrigger pdOscFormatParseTrigger;
    pdOscFormatParseTrigger.depth = 1;
    pdOscFormatParseTrigger.branchIndex = 27;
    pdOscFormatParseTrigger.pdDurationSeconds = 0.3f;
    pdOscFormatParseTrigger.pdPatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 86;\n"
        "#X obj 48 122 oscformat otherware pitch;\n"
        "#X obj 48 162 oscparse;\n"
        "#X obj 48 202 list trim;\n"
        "#X obj 48 242 route otherware;\n"
        "#X obj 48 282 route pitch;\n"
        "#X obj 48 322 mtof;\n"
        "#X obj 48 362 osc~;\n"
        "#X obj 48 402 *~ 0.2;\n"
        "#X obj 48 442 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 9 0 10 1;\n";

    ScDiscAudioEngine oscFormatParseEngine;
    oscFormatParseEngine.prepare (44100.0, 512, 2);
    oscFormatParseEngine.trigger (pdOscFormatParseTrigger);

    juce::AudioBuffer<float> oscFormatParseBuffer (2, 512);
    auto oscFormatParsePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        oscFormatParseEngine.render (oscFormatParseBuffer);

        for (int channel = 0; channel < oscFormatParseBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = oscFormatParseBuffer.getReadPointer (channel);

            for (int i = 0; i < oscFormatParseBuffer.getNumSamples(); ++i)
                oscFormatParsePeak = std::max (oscFormatParsePeak, std::abs (samples[i]));
        }
    }

    oscFormatParseEngine.release();

    if (oscFormatParsePeak <= 0.0005f)
    {
        std::cerr << "Pd oscformat/oscparse fixture produced no audio; peak="
                  << oscFormatParsePeak << '\n';
        return 32;
    }

    std::cout << "oscFormatParsePeak=" << oscFormatParsePeak << '\n';

    DiscAudioTrigger pdFudiFormatParseTrigger;
    pdFudiFormatParseTrigger.depth = 1;
    pdFudiFormatParseTrigger.branchIndex = 28;
    pdFudiFormatParseTrigger.pdDurationSeconds = 0.3f;
    pdFudiFormatParseTrigger.pdPatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 pitch 88;\n"
        "#X obj 48 122 fudiformat;\n"
        "#X obj 48 162 fudiparse;\n"
        "#X obj 48 202 list trim;\n"
        "#X obj 48 242 route pitch;\n"
        "#X obj 48 282 mtof;\n"
        "#X obj 48 322 osc~;\n"
        "#X obj 48 362 *~ 0.2;\n"
        "#X obj 48 402 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n"
        "#X connect 7 0 8 0;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 0 9 1;\n";

    ScDiscAudioEngine fudiFormatParseEngine;
    fudiFormatParseEngine.prepare (44100.0, 512, 2);
    fudiFormatParseEngine.trigger (pdFudiFormatParseTrigger);

    juce::AudioBuffer<float> fudiFormatParseBuffer (2, 512);
    auto fudiFormatParsePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        fudiFormatParseEngine.render (fudiFormatParseBuffer);

        for (int channel = 0; channel < fudiFormatParseBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = fudiFormatParseBuffer.getReadPointer (channel);

            for (int i = 0; i < fudiFormatParseBuffer.getNumSamples(); ++i)
                fudiFormatParsePeak = std::max (fudiFormatParsePeak, std::abs (samples[i]));
        }
    }

    fudiFormatParseEngine.release();

    if (fudiFormatParsePeak <= 0.0005f)
    {
        std::cerr << "Pd fudiformat/fudiparse fixture produced no audio; peak="
                  << fudiFormatParsePeak << '\n';
        return 33;
    }

    std::cout << "fudiFormatParsePeak=" << fudiFormatParsePeak << '\n';

    DiscAudioTrigger pdExprTrigger;
    pdExprTrigger.depth = 1;
    pdExprTrigger.branchIndex = 29;
    pdExprTrigger.pdDurationSeconds = 0.3f;
    pdExprTrigger.pdPatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 24;\n"
        "#X obj 48 122 expr $f1*2+40;\n"
        "#X obj 48 162 mtof;\n"
        "#X obj 48 202 osc~;\n"
        "#X obj 48 242 expr~ $v1*0.2;\n"
        "#X obj 48 282 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine exprEngine;
    exprEngine.prepare (44100.0, 512, 2);
    exprEngine.trigger (pdExprTrigger);

    juce::AudioBuffer<float> exprBuffer (2, 512);
    auto exprPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        exprEngine.render (exprBuffer);

        for (int channel = 0; channel < exprBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = exprBuffer.getReadPointer (channel);

            for (int i = 0; i < exprBuffer.getNumSamples(); ++i)
                exprPeak = std::max (exprPeak, std::abs (samples[i]));
        }
    }

    exprEngine.release();

    if (exprPeak <= 0.0005f)
    {
        std::cerr << "Pd expr/expr~ fixture produced no audio; peak="
                  << exprPeak << '\n';
        return 34;
    }

    std::cout << "exprPeak=" << exprPeak << '\n';

    DiscAudioTrigger pdTaboscTrigger;
    pdTaboscTrigger.depth = 1;
    pdTaboscTrigger.branchIndex = 30;
    pdTaboscTrigger.pdDurationSeconds = 0.3f;
    pdTaboscTrigger.pdPatch =
        "#N canvas 120 120 760 500 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 \\; otherware_tabosc_wave sinesum 256 1 0.35 0.15;\n"
        "#X obj 360 42 array define otherware_tabosc_wave 259;\n"
        "#X obj 48 142 sig~ 330;\n"
        "#X obj 48 182 tabosc4~ otherware_tabosc_wave;\n"
        "#X obj 48 222 *~ 0.2;\n"
        "#X obj 48 262 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine taboscEngine;
    taboscEngine.prepare (44100.0, 512, 2);
    taboscEngine.trigger (pdTaboscTrigger);

    juce::AudioBuffer<float> taboscBuffer (2, 512);
    auto taboscPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        taboscEngine.render (taboscBuffer);

        for (int channel = 0; channel < taboscBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = taboscBuffer.getReadPointer (channel);

            for (int i = 0; i < taboscBuffer.getNumSamples(); ++i)
                taboscPeak = std::max (taboscPeak, std::abs (samples[i]));
        }
    }

    taboscEngine.release();

    if (taboscPeak <= 0.0005f)
    {
        std::cerr << "Pd tabosc4~ fixture produced no audio; peak="
                  << taboscPeak << '\n';
        return 35;
    }

    std::cout << "taboscPeak=" << taboscPeak << '\n';

    const auto cloneDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getNonexistentChildFile ("otherware-pd-clone-smoke", {}, false);
    cloneDirectory.createDirectory();
    cloneDirectory.getChildFile ("otherwareclonevoice.pd").replaceWithText (
        "#N canvas 80 80 420 300 10;\n"
        "#X obj 35 35 inlet;\n"
        "#X obj 35 75 t b f;\n"
        "#X msg 35 115 0 \\, 0.16 5 \\, 0 260 5;\n"
        "#X obj 35 155 vline~;\n"
        "#X obj 175 75 mtof;\n"
        "#X obj 175 115 osc~;\n"
        "#X obj 175 155 *~;\n"
        "#X obj 175 195 outlet~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 4 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 6 1;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 6 0 7 0;\n");

    DiscAudioTrigger pdCloneTrigger;
    pdCloneTrigger.depth = 1;
    pdCloneTrigger.branchIndex = 31;
    pdCloneTrigger.pdDurationSeconds = 0.35f;
    pdCloneTrigger.pdSearchPath = cloneDirectory.getFullPathName();
    pdCloneTrigger.pdPatch =
        "#N canvas 120 120 640 420 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 all 67;\n"
        "#X obj 48 132 clone otherwareclonevoice 4;\n"
        "#X obj 48 182 *~ 0.2;\n"
        "#X obj 48 222 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 3 0 4 1;\n";

    ScDiscAudioEngine cloneEngine;
    cloneEngine.prepare (44100.0, 512, 2);
    cloneEngine.trigger (pdCloneTrigger);

    juce::AudioBuffer<float> cloneBuffer (2, 512);
    auto clonePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        cloneEngine.render (cloneBuffer);

        for (int channel = 0; channel < cloneBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = cloneBuffer.getReadPointer (channel);

            for (int i = 0; i < cloneBuffer.getNumSamples(); ++i)
                clonePeak = std::max (clonePeak, std::abs (samples[i]));
        }
    }

    cloneEngine.release();
    cloneDirectory.deleteRecursively();

    if (clonePeak <= 0.0005f)
    {
        std::cerr << "Pd clone abstraction fixture produced no audio; peak="
                  << clonePeak << '\n';
        return 36;
    }

    std::cout << "clonePeak=" << clonePeak << '\n';

    const auto soundFileDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                        .getNonexistentChildFile ("otherware-pd-soundfiler-smoke", {}, false);
    soundFileDirectory.createDirectory();

    const auto sampleFile = soundFileDirectory.getChildFile ("otherware-sample.wav");

    if (! writeMonoWavFile (sampleFile))
    {
        std::cerr << "Could not create Pd soundfiler WAV fixture at "
                  << sampleFile.getFullPathName() << '\n';
        return 37;
    }

    const auto samplePath = sampleFile.getFullPathName().replace ("\\", "/");

    DiscAudioTrigger pdSoundfilerTrigger;
    pdSoundfilerTrigger.depth = 1;
    pdSoundfilerTrigger.branchIndex = 32;
    pdSoundfilerTrigger.pdDurationSeconds = 0.35f;
    pdSoundfilerTrigger.pdPatch =
        juce::String ("#N canvas 120 120 760 500 10;\n")
        + "#X obj 48 42 loadbang;\n"
        + "#X obj 48 82 t b b;\n"
        + "#X msg 180 122 read -resize " + samplePath + " otherware_soundfile;\n"
        + "#X obj 180 162 soundfiler;\n"
        + "#X obj 48 122 del 10;\n"
        + "#X obj 360 42 array define otherware_soundfile 2048;\n"
        + "#X obj 48 172 tabplay~ otherware_soundfile;\n"
        + "#X obj 48 212 *~ 0.2;\n"
        + "#X obj 48 252 dac~;\n"
        + "#X connect 0 0 1 0;\n"
        + "#X connect 1 0 4 0;\n"
        + "#X connect 1 1 2 0;\n"
        + "#X connect 2 0 3 0;\n"
        + "#X connect 4 0 6 0;\n"
        + "#X connect 6 0 7 0;\n"
        + "#X connect 7 0 8 0;\n"
        + "#X connect 7 0 8 1;\n";

    ScDiscAudioEngine soundfilerEngine;
    soundfilerEngine.prepare (44100.0, 512, 2);
    soundfilerEngine.trigger (pdSoundfilerTrigger);

    juce::AudioBuffer<float> soundfilerBuffer (2, 512);
    auto soundfilerPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        soundfilerEngine.render (soundfilerBuffer);

        for (int channel = 0; channel < soundfilerBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = soundfilerBuffer.getReadPointer (channel);

            for (int i = 0; i < soundfilerBuffer.getNumSamples(); ++i)
                soundfilerPeak = std::max (soundfilerPeak, std::abs (samples[i]));
        }
    }

    soundfilerEngine.release();
    soundFileDirectory.deleteRecursively();

    if (soundfilerPeak <= 0.0005f)
    {
        std::cerr << "Pd soundfiler/tabplay~ fixture produced no audio; peak="
                  << soundfilerPeak << '\n';
        return 38;
    }

    std::cout << "soundfilerPeak=" << soundfilerPeak << '\n';

    const auto readsfDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getNonexistentChildFile ("otherware-pd-readsf-smoke", {}, false);
    readsfDirectory.createDirectory();

    const auto readsfFile = readsfDirectory.getChildFile ("otherware-stream.wav");

    if (! writeMonoWavFile (readsfFile))
    {
        std::cerr << "Could not create Pd readsf~ WAV fixture at "
                  << readsfFile.getFullPathName() << '\n';
        return 39;
    }

    const auto readsfPath = readsfFile.getFullPathName().replace ("\\", "/");

    DiscAudioTrigger pdReadsfTrigger;
    pdReadsfTrigger.depth = 1;
    pdReadsfTrigger.branchIndex = 33;
    pdReadsfTrigger.pdDurationSeconds = 0.35f;
    pdReadsfTrigger.pdPatch =
        juce::String ("#N canvas 120 120 700 420 10;\n")
        + "#X obj 48 42 loadbang;\n"
        + "#X msg 48 82 open " + readsfPath + " \\, start;\n"
        + "#X obj 48 132 readsf~ 1;\n"
        + "#X obj 48 182 *~ 0.25;\n"
        + "#X obj 48 222 dac~;\n"
        + "#X connect 0 0 1 0;\n"
        + "#X connect 1 0 2 0;\n"
        + "#X connect 2 0 3 0;\n"
        + "#X connect 3 0 4 0;\n"
        + "#X connect 3 0 4 1;\n";

    ScDiscAudioEngine readsfEngine;
    readsfEngine.prepare (44100.0, 512, 2);
    readsfEngine.trigger (pdReadsfTrigger);

    juce::AudioBuffer<float> readsfBuffer (2, 512);
    auto readsfPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        readsfEngine.render (readsfBuffer);

        for (int channel = 0; channel < readsfBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = readsfBuffer.getReadPointer (channel);

            for (int i = 0; i < readsfBuffer.getNumSamples(); ++i)
                readsfPeak = std::max (readsfPeak, std::abs (samples[i]));
        }
    }

    readsfEngine.release();
    readsfDirectory.deleteRecursively();

    if (readsfPeak <= 0.0005f)
    {
        std::cerr << "Pd readsf~ fixture produced no audio; peak="
                  << readsfPeak << '\n';
        return 40;
    }

    std::cout << "readsfPeak=" << readsfPeak << '\n';

    if (std::getenv ("OTHERWARE_RUN_PD_WRITESF_SMOKE") != nullptr)
    {
        const auto writesfDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                          .getNonexistentChildFile ("otherware-pd-writesf-smoke", {}, false);
        writesfDirectory.createDirectory();

        const auto writesfFile = writesfDirectory.getChildFile ("otherware-recorded.wav");
        writesfFile.deleteFile();
        const auto writesfPath = writesfFile.getFullPathName().replace ("\\", "/");

        DiscAudioTrigger pdWritesfTrigger;
        pdWritesfTrigger.depth = 1;
        pdWritesfTrigger.branchIndex = 34;
        pdWritesfTrigger.pdDurationSeconds = 0.35f;
        pdWritesfTrigger.pdPatch =
            juce::String ("#N canvas 120 120 760 460 10;\n")
            + "#X obj 48 42 loadbang;\n"
            + "#X msg 48 82 open " + writesfPath + " \\, start;\n"
            + "#X obj 252 82 del 180;\n"
            + "#X msg 252 122 stop;\n"
            + "#X obj 48 172 osc~ 440;\n"
            + "#X obj 48 212 *~ 0.25;\n"
            + "#X obj 48 252 writesf~ 1;\n"
            + "#X connect 0 0 1 0;\n"
            + "#X connect 0 0 2 0;\n"
            + "#X connect 1 0 6 0;\n"
            + "#X connect 2 0 3 0;\n"
            + "#X connect 3 0 6 0;\n"
            + "#X connect 4 0 5 0;\n"
            + "#X connect 5 0 6 0;\n";

        ScDiscAudioEngine writesfEngine;
        writesfEngine.prepare (44100.0, 512, 2);
        writesfEngine.trigger (pdWritesfTrigger);

        juce::AudioBuffer<float> writesfBuffer (2, 512);

        for (int block = 0; block < 80; ++block)
            writesfEngine.render (writesfBuffer);

        writesfEngine.release();

        const auto writesfPeak = readPcm16WavPeak (writesfFile);

        if (writesfPeak <= 0.0005f)
        {
            std::cerr << "Pd writesf~ fixture wrote no readable audio; peak="
                      << writesfPeak << " fileSize=" << writesfFile.getSize() << '\n';
            writesfDirectory.deleteRecursively();
            return 41;
        }

        std::cout << "writesfPeak=" << writesfPeak << '\n';
        writesfDirectory.deleteRecursively();
    }
    else
    {
        std::cout << "writesfPeak=skipped\n";
    }

    DiscAudioTrigger pdAudioSubpatchTrigger;
    pdAudioSubpatchTrigger.depth = 1;
    pdAudioSubpatchTrigger.branchIndex = 35;
    pdAudioSubpatchTrigger.pdDurationSeconds = 0.3f;
    pdAudioSubpatchTrigger.pdPatch =
        "#N canvas 120 120 700 460 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X msg 48 82 1;\n"
        "#X obj 210 82 sig~ 440;\n"
        "#N canvas 220 220 420 300 switched_voice 0;\n"
        "#X obj 42 42 inlet~;\n"
        "#X obj 42 82 osc~;\n"
        "#X obj 42 122 outlet~;\n"
        "#X obj 180 42 inlet;\n"
        "#X obj 180 82 switch~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X restore 210 132 pd switched_voice;\n"
        "#X obj 210 192 *~ 0.2;\n"
        "#X obj 210 232 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 3 1;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";

    ScDiscAudioEngine audioSubpatchEngine;
    audioSubpatchEngine.prepare (44100.0, 512, 2);
    audioSubpatchEngine.trigger (pdAudioSubpatchTrigger);

    juce::AudioBuffer<float> audioSubpatchBuffer (2, 512);
    auto audioSubpatchPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        audioSubpatchEngine.render (audioSubpatchBuffer);

        for (int channel = 0; channel < audioSubpatchBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = audioSubpatchBuffer.getReadPointer (channel);

            for (int i = 0; i < audioSubpatchBuffer.getNumSamples(); ++i)
                audioSubpatchPeak = std::max (audioSubpatchPeak, std::abs (samples[i]));
        }
    }

    audioSubpatchEngine.release();

    if (audioSubpatchPeak <= 0.0005f)
    {
        std::cerr << "Pd audio subpatch/switch~ fixture produced no audio; peak="
                  << audioSubpatchPeak << '\n';
        return 42;
    }

    std::cout << "audioSubpatchPeak=" << audioSubpatchPeak << '\n';

    DiscAudioTrigger pdTabSendReceiveTrigger;
    pdTabSendReceiveTrigger.depth = 1;
    pdTabSendReceiveTrigger.branchIndex = 36;
    pdTabSendReceiveTrigger.pdDurationSeconds = 0.3f;
    pdTabSendReceiveTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 osc~ 550;\n"
        "#X obj 48 82 tabsend~ otherware_block_buffer;\n"
        "#X obj 360 42 array define otherware_block_buffer 64;\n"
        "#X obj 48 142 tabreceive~ otherware_block_buffer;\n"
        "#X obj 48 182 *~ 0.2;\n"
        "#X obj 48 222 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n";

    ScDiscAudioEngine tabSendReceiveEngine;
    tabSendReceiveEngine.prepare (44100.0, 512, 2);
    tabSendReceiveEngine.trigger (pdTabSendReceiveTrigger);

    juce::AudioBuffer<float> tabSendReceiveBuffer (2, 512);
    auto tabSendReceivePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        tabSendReceiveEngine.render (tabSendReceiveBuffer);

        for (int channel = 0; channel < tabSendReceiveBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = tabSendReceiveBuffer.getReadPointer (channel);

            for (int i = 0; i < tabSendReceiveBuffer.getNumSamples(); ++i)
                tabSendReceivePeak = std::max (tabSendReceivePeak, std::abs (samples[i]));
        }
    }

    tabSendReceiveEngine.release();

    if (tabSendReceivePeak <= 0.0005f)
    {
        std::cerr << "Pd tabsend~/tabreceive~ fixture produced no audio; peak="
                  << tabSendReceivePeak << '\n';
        return 43;
    }

    std::cout << "tabSendReceivePeak=" << tabSendReceivePeak << '\n';

    DiscAudioTrigger pdSamplerateTrigger;
    pdSamplerateTrigger.depth = 1;
    pdSamplerateTrigger.branchIndex = 37;
    pdSamplerateTrigger.pdDurationSeconds = 0.3f;
    pdSamplerateTrigger.pdPatch =
        "#N canvas 120 120 680 420 10;\n"
        "#X obj 48 42 loadbang;\n"
        "#X obj 48 82 samplerate~;\n"
        "#X obj 48 122 / 100;\n"
        "#X obj 48 162 sig~;\n"
        "#X obj 48 202 osc~;\n"
        "#X obj 48 242 *~ 0.2;\n"
        "#X obj 48 282 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 5 0 6 0;\n"
        "#X connect 5 0 6 1;\n";

    ScDiscAudioEngine samplerateEngine;
    samplerateEngine.prepare (44100.0, 512, 2);
    samplerateEngine.trigger (pdSamplerateTrigger);

    juce::AudioBuffer<float> samplerateBuffer (2, 512);
    auto sampleratePeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        samplerateEngine.render (samplerateBuffer);

        for (int channel = 0; channel < samplerateBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = samplerateBuffer.getReadPointer (channel);

            for (int i = 0; i < samplerateBuffer.getNumSamples(); ++i)
                sampleratePeak = std::max (sampleratePeak, std::abs (samples[i]));
        }
    }

    samplerateEngine.release();

    if (sampleratePeak <= 0.0005f)
    {
        std::cerr << "Pd samplerate~ fixture produced no audio; peak="
                  << sampleratePeak << '\n';
        return 44;
    }

    std::cout << "sampleratePeak=" << sampleratePeak << '\n';

    DiscAudioTrigger pdNonMultipleBlockTrigger;
    pdNonMultipleBlockTrigger.depth = 1;
    pdNonMultipleBlockTrigger.branchIndex = 52;
    pdNonMultipleBlockTrigger.pdDurationSeconds = 0.002f;
    pdNonMultipleBlockTrigger.pdPatch =
        "#N canvas 120 120 360 240 10;\n"
        "#X obj 48 42 osc~ 330;\n"
        "#X obj 48 82 *~ 0.2;\n"
        "#X obj 48 122 dac~;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 0 2 1;\n";

    ScDiscAudioEngine nonMultipleBlockEngine;
    nonMultipleBlockEngine.prepare (44100.0, 100, 2);
    nonMultipleBlockEngine.trigger (pdNonMultipleBlockTrigger);

    juce::AudioBuffer<float> nonMultipleBlockBuffer (2, 100);
    nonMultipleBlockEngine.render (nonMultipleBlockBuffer);

    auto nonMultipleBlockPeak = 0.0f;

    for (int channel = 0; channel < nonMultipleBlockBuffer.getNumChannels(); ++channel)
    {
        const auto* samples = nonMultipleBlockBuffer.getReadPointer (channel);

        for (int i = 0; i < nonMultipleBlockBuffer.getNumSamples(); ++i)
            nonMultipleBlockPeak = std::max (nonMultipleBlockPeak, std::abs (samples[i]));
    }

    nonMultipleBlockBuffer.clear();
    nonMultipleBlockEngine.render (nonMultipleBlockBuffer);

    auto nonMultipleBlockTailPeak = 0.0f;

    for (int channel = 0; channel < nonMultipleBlockBuffer.getNumChannels(); ++channel)
    {
        const auto* samples = nonMultipleBlockBuffer.getReadPointer (channel);

        for (int i = 0; i < nonMultipleBlockBuffer.getNumSamples(); ++i)
            nonMultipleBlockTailPeak = std::max (nonMultipleBlockTailPeak, std::abs (samples[i]));
    }

    nonMultipleBlockEngine.release();

    if (nonMultipleBlockPeak <= 0.0005f || nonMultipleBlockTailPeak > 0.0001f)
    {
        std::cerr << "Pd non-multiple host buffer fixture failed; peak="
                  << nonMultipleBlockPeak << " tail=" << nonMultipleBlockTailPeak << '\n';
        return 45;
    }

    std::cout << "nonMultipleBlockPeak=" << nonMultipleBlockPeak
              << " tail=" << nonMultipleBlockTailPeak << '\n';

    DiscAudioTrigger pdAdcTrigger;
    pdAdcTrigger.depth = 1;
    pdAdcTrigger.branchIndex = 53;
    pdAdcTrigger.pdDurationSeconds = 0.3f;
    pdAdcTrigger.pdPatch =
        "#N canvas 120 120 420 260 10;\n"
        "#X obj 48 42 adc~ 1 2;\n"
        "#X obj 48 82 *~ 0.5;\n"
        "#X obj 180 82 *~ 0.5;\n"
        "#X obj 48 132 dac~ 1 2;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 0 1 2 0;\n"
        "#X connect 1 0 3 0;\n"
        "#X connect 2 0 3 1;\n";

    ScDiscAudioEngine adcEngine;
    adcEngine.prepare (44100.0, 512, 2, 2);

    if (! adcEngine.isPdAudioInputMuted())
    {
        std::cerr << "Pd host input should be muted by default\n";
        return 107;
    }

    adcEngine.setPdAudioInputMuted (false);
    adcEngine.trigger (pdAdcTrigger);

    juce::AudioBuffer<float> adcInput (2, 512);
    juce::AudioBuffer<float> adcOutput (2, 512);
    adcInput.clear();
    adcInput.addFrom (0, 0, std::vector<float> (512, 0.24f).data(), 512);
    adcInput.addFrom (1, 0, std::vector<float> (512, -0.16f).data(), 512);
    auto adcLeftPeak = 0.0f;
    auto adcRightPeak = 0.0f;

    for (int block = 0; block < 8; ++block)
    {
        adcEngine.render (adcOutput, &adcInput);

        for (int sample = 0; sample < adcOutput.getNumSamples(); ++sample)
        {
            adcLeftPeak = std::max (adcLeftPeak, std::abs (adcOutput.getSample (0, sample)));
            adcRightPeak = std::max (adcRightPeak, std::abs (adcOutput.getSample (1, sample)));
        }
    }

    adcEngine.release();

    if (adcLeftPeak < 0.10f || adcRightPeak < 0.06f)
    {
        std::cerr << "Pd adc~ host-input fixture produced no routed audio; left="
                  << adcLeftPeak << " right=" << adcRightPeak << '\n';
        return 107;
    }

    std::cout << "adcInputPeaks=" << adcLeftPeak << "/" << adcRightPeak << '\n';

    DiscAudioTrigger pdFftTrigger;
    pdFftTrigger.depth = 1;
    pdFftTrigger.branchIndex = 51;
    pdFftTrigger.pdDurationSeconds = 0.3f;
    pdFftTrigger.pdPatch =
        "#N canvas 120 120 760 460 10;\n"
        "#X obj 48 42 osc~ 440;\n"
        "#X obj 48 82 rfft~;\n"
        "#X obj 48 122 rifft~;\n"
        "#X obj 48 162 /~ 512;\n"
        "#X obj 48 202 *~ 0.15;\n"
        "#X obj 48 242 dac~;\n"
        "#X obj 330 42 osc~ 660;\n"
        "#X obj 480 42 sig~ 0;\n"
        "#X obj 330 82 fft~;\n"
        "#X obj 330 122 ifft~;\n"
        "#X obj 330 162 /~ 512;\n"
        "#X obj 330 202 *~ 0.08;\n"
        "#X obj 610 82 framp~;\n"
        "#X obj 610 122 -~ 0.5;\n"
        "#X obj 610 162 *~ 0.02;\n"
        "#X connect 0 0 1 0;\n"
        "#X connect 1 0 2 0;\n"
        "#X connect 1 1 2 1;\n"
        "#X connect 2 0 3 0;\n"
        "#X connect 3 0 4 0;\n"
        "#X connect 4 0 5 0;\n"
        "#X connect 4 0 5 1;\n"
        "#X connect 6 0 8 0;\n"
        "#X connect 7 0 8 1;\n"
        "#X connect 8 0 9 0;\n"
        "#X connect 8 1 9 1;\n"
        "#X connect 9 0 10 0;\n"
        "#X connect 10 0 11 0;\n"
        "#X connect 11 0 5 0;\n"
        "#X connect 11 0 5 1;\n"
        "#X connect 12 0 13 0;\n"
        "#X connect 13 0 14 0;\n"
        "#X connect 14 0 5 0;\n"
        "#X connect 14 0 5 1;\n";

    ScDiscAudioEngine fftEngine;
    fftEngine.prepare (44100.0, 512, 2);
    fftEngine.trigger (pdFftTrigger);

    juce::AudioBuffer<float> fftBuffer (2, 512);
    auto fftPeak = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        fftEngine.render (fftBuffer);

        for (int channel = 0; channel < fftBuffer.getNumChannels(); ++channel)
        {
            const auto* samples = fftBuffer.getReadPointer (channel);

            for (int i = 0; i < fftBuffer.getNumSamples(); ++i)
                fftPeak = std::max (fftPeak, std::abs (samples[i]));
        }
    }

    fftEngine.release();

    if (fftPeak <= 0.0005f)
    {
        std::cerr << "Pd FFT/rFFT/framp fixture produced no audio; peak="
                  << fftPeak << '\n';
        return 105;
    }

    std::cout << "fftPeak=" << fftPeak << '\n';

    return 0;
}
