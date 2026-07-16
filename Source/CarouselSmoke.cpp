#include "CarouselEditorComponent.h"

#include <cmath>
#include <iostream>

int main()
{
    auto source = CarouselDocument::createDefault();
    source.columns = 19; source.rows = 11; source.bpm = 137.0;
    source.items.front().radius = 2.25f; source.items.front().speed = -0.5f; source.items.front().euclidean = true;
    const auto restored = CarouselDocument::fromValueTree (source.toValueTree());
    if (restored.columns != 19 || restored.rows != 11 || std::abs (restored.bpm - 137.0) > 0.001
        || restored.items.size() != source.items.size() || std::abs (restored.items.front().radius - 2.25f) > 0.001f
        || std::abs (restored.items.front().speed + 0.5f) > 0.001f || ! restored.items.front().euclidean)
    {
        std::cerr << "Carousel document round-trip failed\n";
        return 1;
    }
    std::cout << "carouselDocument=" << restored.items.size() << " objects\n";
    return 0;
}
