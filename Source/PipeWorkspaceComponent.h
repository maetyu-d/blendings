#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace otherware
{
struct PipeWorkspaceDiscTrigger
{
    enum class PlaybackType { synth = 0, superCollider, pureData };
    PlaybackType playback = PlaybackType::synth;
    int midiNote = 60;
    float durationSeconds = 0.45f;
    juce::String source;
};

std::unique_ptr<juce::Component> createPipeWorkspaceComponent();
juce::var createPipeWorkspaceState (juce::Component& component);
bool applyPipeWorkspaceState (juce::Component& component, const juce::var& state);
void setPipeWorkspaceTempo (juce::Component& component, double bpm);
void setPipeWorkspaceSampleClock (juce::Component& component, std::function<double()> clockSeconds);
void setPipeWorkspaceRunning (juce::Component& component, bool running);
void setPipeWorkspaceChangeCallback (juce::Component& component, std::function<void()> callback);
void setPipeWorkspaceDiscTriggerCallback (juce::Component& component,
                                          std::function<void(const PipeWorkspaceDiscTrigger&)> callback);
using PipeWorkspacePdCommit = std::function<void (const juce::String&, float)>;
using PipeWorkspacePdEditorCallback = std::function<void (const juce::String&, float, PipeWorkspacePdCommit)>;
void setPipeWorkspacePdEditorCallback (juce::Component& component, PipeWorkspacePdEditorCallback callback);
using PipeWorkspaceScCommit = std::function<void (const juce::String&, float)>;
using PipeWorkspaceScEditorCallback = std::function<void (const juce::String&, float, PipeWorkspaceScCommit)>;
void setPipeWorkspaceScEditorCallback (juce::Component& component, PipeWorkspaceScEditorCallback callback);
bool runPipeWorkspaceSmokeChecks (juce::String& failureMessage);
}
