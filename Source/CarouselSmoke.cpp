#include "CarouselEditorComponent.h"

#include <cmath>
#include <iostream>

int main()
{
    auto source = CarouselDocument::createDefault();
    source.columns = 19; source.rows = 11; source.bpm = 137.0;
    source.items.front().radius = 2.25f; source.items.front().speed = -0.5f; source.items.front().euclidean = true;
    source.items[1].playback = CarouselDocument::PlaybackType::superCollider;
    source.items[1].durationSeconds = 1.75f;
    source.items[1].scCode = "Out.ar(out, SinOsc.ar(pitch.midicps) * 0.1);";
    source.items[2].playback = CarouselDocument::PlaybackType::pureData;
    source.items[2].pdPatch = "#N canvas 0 0 200 120 10;\n#X obj 20 20 r trigger;\n";
    const auto restored = CarouselDocument::fromValueTree (source.toValueTree());
    if (restored.columns != 19 || restored.rows != 11 || std::abs (restored.bpm - 137.0) > 0.001
        || restored.items.size() != source.items.size() || std::abs (restored.items.front().radius - 2.25f) > 0.001f
        || std::abs (restored.items.front().speed + 0.5f) > 0.001f || ! restored.items.front().euclidean
        || restored.items[1].playback != CarouselDocument::PlaybackType::superCollider
        || std::abs (restored.items[1].durationSeconds - 1.75f) > 0.001f
        || restored.items[1].scCode != source.items[1].scCode
        || restored.items[2].playback != CarouselDocument::PlaybackType::pureData
        || restored.items[2].pdPatch != source.items[2].pdPatch)
    {
        std::cerr << "Carousel document round-trip failed\n";
        return 1;
    }
    std::cout << "carouselDocument=" << restored.items.size() << " objects\n";
    return 0;
}
