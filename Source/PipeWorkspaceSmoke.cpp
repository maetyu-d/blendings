#include "PipeWorkspaceComponent.h"

#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::String failure;
    if (! otherware::runPipeWorkspaceSmokeChecks (failure))
    {
        std::cerr << failure << '\n';
        return 1;
    }

    std::cout << "Pipe World smoke checks passed\n";
    return 0;
}
