#include "CarouselEditorComponent.h"

#include <cmath>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    auto source = CarouselDocument::createDefault();
    source.columns = 19; source.rows = 11; source.bpm = 137.0;
    source.items.front().radius = 2.25f; source.items.front().speed = -0.5f; source.items.front().euclidean = true;
    source.items[1].playback = CarouselDocument::PlaybackType::superCollider;
    source.items[1].durationSeconds = 1.75f;
    source.items[1].scCode = "Out.ar(out, SinOsc.ar(pitch.midicps) * 0.1);";
    source.items[2].playback = CarouselDocument::PlaybackType::pureData;
    source.items[2].pdPatch = "#N canvas 0 0 200 120 10;\n#X obj 20 20 r trigger;\n";
    source.items[4].ownerOrbit = source.items.front().id;
    CarouselDocument::Item plank;
    plank.id = source.nextId++; plank.type = CarouselDocument::ItemType::plank;
    plank.x = 7.5f; plank.y = 3.75f; plank.ownerOrbit = source.items.front().id;
    plank.radius = 3.58f; plank.phase = 0.21f; plank.midi = 69;
    source.items.push_back (plank);
    CarouselDocument::Item attachedCarousel;
    attachedCarousel.id = source.nextId++; attachedCarousel.type = CarouselDocument::ItemType::orbit;
    attachedCarousel.x = plank.x; attachedCarousel.y = plank.y; attachedCarousel.ownerOrbit = plank.id;
    source.items.push_back (attachedCarousel);
    CarouselDocument::Item mountedTone;
    mountedTone.id = source.nextId++; mountedTone.type = CarouselDocument::ItemType::tone;
    mountedTone.x = plank.x; mountedTone.y = plank.y; mountedTone.ownerOrbit = plank.id;
    mountedTone.playback = CarouselDocument::PlaybackType::superCollider;
    mountedTone.scCode = "Out.ar(out, Pulse.ar(pitch.midicps) * 0.08);";
    source.items.push_back (mountedTone);
    const auto restored = CarouselDocument::fromValueTree (source.toValueTree());
    auto damagedTree = source.toValueTree();
    damagedTree.getChild (1).setProperty ("owner", 99999, nullptr);
    const auto recovered = CarouselDocument::fromValueTree (damagedTree);
    if (recovered.items[1].ownerOrbit != -1 || ! recovered.recoveryNotice.containsIgnoreCase ("Recovered"))
    {
        std::cerr << "Carousel did not recover an invalid parent attachment\n";
        return 1;
    }
    if (source.items[1].ownerOrbit != source.items.front().id
        || restored.columns != 19 || restored.rows != 11 || std::abs (restored.bpm - 137.0) > 0.001
        || restored.items.size() != source.items.size() || std::abs (restored.items.front().radius - 2.25f) > 0.001f
        || std::abs (restored.items.front().speed + 0.5f) > 0.001f || ! restored.items.front().euclidean
        || restored.items[1].playback != CarouselDocument::PlaybackType::superCollider
        || std::abs (restored.items[1].durationSeconds - 1.75f) > 0.001f
        || restored.items[1].scCode != source.items[1].scCode
        || restored.items[2].playback != CarouselDocument::PlaybackType::pureData
        || restored.items[2].pdPatch != source.items[2].pdPatch
        || restored.items[4].type != CarouselDocument::ItemType::post
        || restored.items[4].ownerOrbit != -1
        || restored.items[restored.items.size() - 3].type != CarouselDocument::ItemType::plank
        || restored.items[restored.items.size() - 3].ownerOrbit != source.items.front().id
        || std::abs (restored.items[restored.items.size() - 3].radius - 3.58f) > 0.001f
        || std::abs (restored.items[restored.items.size() - 3].phase - 0.21f) > 0.001f
        || restored.items[restored.items.size() - 2].type != CarouselDocument::ItemType::orbit
        || restored.items[restored.items.size() - 2].ownerOrbit != plank.id
        || restored.items.back().type != CarouselDocument::ItemType::tone
        || restored.items.back().ownerOrbit != plank.id
        || restored.items.back().scCode != mountedTone.scCode)
    {
        std::cerr << "Carousel document round-trip failed\n";
        return 1;
    }

    MusicalObjectEditorComponent soundChooser (MusicalObjectSound {});
    auto* playbackChoice = [&]() -> juce::ComboBox*
    {
        for (auto* child : soundChooser.getChildren())
            if (auto* combo = dynamic_cast<juce::ComboBox*> (child)) return combo;
        return nullptr;
    }();
    auto editorRequest = MusicalObjectSound::Playback::synth;
    soundChooser.onOpenEditor = [&editorRequest] (const MusicalObjectSound& sound)
    {
        editorRequest = sound.playback;
    };
    const auto clickEditorButton = [&soundChooser] (const juce::String& caption)
    {
        for (auto* child : soundChooser.getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr
                && button->getButtonText() == caption && button->isVisible())
            {
                if (button->onClick) button->onClick();
                return true;
            }
        return false;
    };
    if (playbackChoice == nullptr)
    {
        std::cerr << "Carousel sound chooser has no playback selector\n";
        return 1;
    }
    playbackChoice->setSelectedItemIndex (1, juce::sendNotificationSync);
    if (! clickEditorButton ("Open SC") || editorRequest != MusicalObjectSound::Playback::superCollider)
    {
        std::cerr << "Carousel sound chooser did not expose its SC editor button\n";
        return 1;
    }
    playbackChoice->setSelectedItemIndex (2, juce::sendNotificationSync);
    if (! clickEditorButton ("Open Pd") || editorRequest != MusicalObjectSound::Playback::pureData)
    {
        std::cerr << "Carousel sound chooser did not expose its Pd editor button\n";
        return 1;
    }

    CarouselEditorComponent editor;
    editor.setDocument (restored);
    auto scOpened = false;
    auto pdOpened = false;
    editor.onScEditorRequested = [&scOpened] (const juce::String& sourceText, float duration,
                                               CarouselEditorComponent::SoundCommit commit)
    {
        scOpened = sourceText.contains ("SinOsc") && duration > 0.0f;
        commit (sourceText + "\n// full editor", 2.5f);
    };
    editor.onPdEditorRequested = [&pdOpened] (const juce::String& patch, float duration,
                                               CarouselEditorComponent::SoundCommit commit)
    {
        pdOpened = patch.startsWith ("#N canvas") && duration > 0.0f;
        commit (patch + "#X text 20 60 full editor;\n", 3.0f);
    };
    if (! editor.openFullSoundEditor (restored.items[1].id)
        || ! editor.openFullSoundEditor (restored.items[2].id)
        || ! scOpened || ! pdOpened)
    {
        std::cerr << "Carousel did not request its full sound editors\n";
        return 1;
    }
    const auto edited = editor.getDocument();
    if (! edited.items[1].scCode.contains ("full editor")
        || std::abs (edited.items[1].durationSeconds - 2.5f) > 0.001f
        || ! edited.items[2].pdPatch.contains ("full editor")
        || std::abs (edited.items[2].durationSeconds - 3.0f) > 0.001f)
    {
        std::cerr << "Carousel did not persist full sound editor changes\n";
        return 1;
    }
    if (! editor.undo())
    {
        std::cerr << "Carousel undo was unavailable after a sound edit\n";
        return 1;
    }
    const auto undone = editor.getDocument();
    if (! undone.items[1].scCode.contains ("full editor")
        || undone.items[2].pdPatch.contains ("full editor"))
    {
        std::cerr << "Carousel undo did not restore the previous attachment document state\n";
        return 1;
    }
    if (! editor.redo() || ! editor.getDocument().items[2].pdPatch.contains ("full editor"))
    {
        std::cerr << "Carousel redo did not restore the sound edit\n";
        return 1;
    }
    juce::String performanceFailure;
    juce::String attachmentFailure;
    if (! editor.runAttachmentSmokeChecks (attachmentFailure))
    {
        std::cerr << attachmentFailure << '\n';
        return 1;
    }

    juce::String visualFailure;
    if (! editor.runVisualInteractionSmokeChecks (visualFailure))
    {
        std::cerr << visualFailure << std::endl;
        return 1;
    }
    if (! editor.runPerformanceSmokeChecks (performanceFailure))
    {
        std::cerr << performanceFailure << '\n';
        return 1;
    }
    std::cout << "carouselDocument=" << restored.items.size() << " objects\n";
    return 0;
}
