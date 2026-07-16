#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace otherware
{
std::unique_ptr<juce::Component> createPipeWorkspaceComponent();
juce::var createPipeWorkspaceState (juce::Component& component);
bool applyPipeWorkspaceState (juce::Component& component, const juce::var& state);
void setPipeWorkspaceTempo (juce::Component& component, double bpm);
void setPipeWorkspaceRunning (juce::Component& component, bool running);
void setPipeWorkspaceChangeCallback (juce::Component& component, std::function<void()> callback);
}
