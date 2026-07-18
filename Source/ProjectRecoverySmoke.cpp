#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <cmath>
#include <iostream>

namespace
{
bool finitePoint (const juce::ValueTree& point)
{
    const auto x = static_cast<double> (point.getProperty ("x", 0.0));
    const auto y = static_cast<double> (point.getProperty ("y", 0.0));
    return std::isfinite (x) && std::isfinite (y);
}

juce::StringArray validate (const juce::ValueTree& root)
{
    juce::StringArray report;
    if (! root.isValid() || ! root.hasType ("blendingsProject")) report.add ("invalid root");
    if (! root.hasProperty ("version")) report.add ("missing version");
    if (const auto routes = root.getChildWithName ("routes"); routes.isValid())
        for (const auto route : routes)
        {
            if (route.getNumChildren() < 2) report.add ("short route");
            for (const auto point : route) if (! finitePoint (point)) report.add ("invalid coordinate");
        }
    return report;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto truncated = juce::XmlDocument::parse ("<blendingsProject version=\"1\"><routes>");
    if (truncated != nullptr) return 1;

    juce::ValueTree missing ("blendingsProject");
    missing.setProperty ("version", 1, nullptr);
    if (! validate (missing).isEmpty()) return 2;

    juce::ValueTree damaged ("blendingsProject"); damaged.setProperty ("version", 1, nullptr);
    juce::ValueTree routes ("routes"), route ("route"), point ("point");
    point.setProperty ("x", std::numeric_limits<double>::infinity(), nullptr);
    point.setProperty ("y", 10.0, nullptr); route.addChild (point, -1, nullptr);
    routes.addChild (route, -1, nullptr); damaged.addChild (routes, -1, nullptr);
    const auto report = validate (damaged);
    if (! report.contains ("short route") || ! report.contains ("invalid coordinate")) return 3;

    auto temporary = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("blendings-recovery", ".otherware");
    auto backup = temporary.getSiblingFile (temporary.getFileNameWithoutExtension() + "-recovery-backup.otherware");
    temporary.replaceWithText ("original");
    if (! temporary.copyFileTo (backup)) return 4;
    temporary.replaceWithText ("changed");
    if (backup.loadFileAsString() != "original") return 5;
    temporary.deleteFile(); backup.deleteFile();
    std::cout << "Project recovery smoke passed\n";
    return 0;
}
