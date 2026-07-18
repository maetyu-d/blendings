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
    if (! root.isValid() || ! root.hasType ("otherwareProject")) report.add ("invalid root");
    if (! root.hasProperty ("version")) report.add ("missing version");
    if (const auto routes = root.getChildWithName ("routes"); routes.isValid())
        for (const auto route : routes)
        {
            if (route.getNumChildren() < 2) report.add ("short route");
            for (const auto point : route) if (! finitePoint (point)) report.add ("invalid coordinate");
        }
    juce::StringArray sourceIds, targetIds, teleportIds;
    if (const auto modulation = root.getChildWithName ("modulation"); modulation.isValid())
        for (const auto child : modulation)
            if (child.hasType ("source")) sourceIds.add (child["id"].toString());
    for (const auto sectionName : { "discs", "pipeTaps", "pipeDrains", "pipeCloners", "pipeSpeedLimits",
                                    "pipeWaits", "pipeStrikes", "pipeTeleports", "pipeFilters", "pipeLogics" })
        if (const auto section = root.getChildWithName (sectionName); section.isValid())
            for (const auto child : section)
            {
                targetIds.add (child["id"].toString());
                if (child.hasType ("teleport")) teleportIds.add (child["id"].toString());
            }
    if (const auto modulation = root.getChildWithName ("modulation"); modulation.isValid())
        for (const auto child : modulation)
            if (child.hasType ("connection")
                && (! sourceIds.contains (child["source"].toString()) || ! targetIds.contains (child["target"].toString())))
                report.add ("orphan modulation");
    if (const auto teleports = root.getChildWithName ("pipeTeleports"); teleports.isValid())
        for (const auto child : teleports)
            if (child["destination"].toString().isNotEmpty() && ! teleportIds.contains (child["destination"].toString()))
                report.add ("orphan teleport");
    return report;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto truncated = juce::XmlDocument::parse ("<otherwareProject version=\"1\"><routes>");
    if (truncated != nullptr) return 1;

    juce::ValueTree missing ("otherwareProject");
    missing.setProperty ("version", 1, nullptr);
    if (! validate (missing).isEmpty()) return 2;

    juce::ValueTree damaged ("otherwareProject"); damaged.setProperty ("version", 1, nullptr);
    juce::ValueTree routes ("routes"), route ("route"), point ("point");
    point.setProperty ("x", std::numeric_limits<double>::infinity(), nullptr);
    point.setProperty ("y", 10.0, nullptr); route.addChild (point, -1, nullptr);
    routes.addChild (route, -1, nullptr); damaged.addChild (routes, -1, nullptr);
    const auto report = validate (damaged);
    if (! report.contains ("short route") || ! report.contains ("invalid coordinate")) return 3;

    juce::ValueTree relationships ("otherwareProject"); relationships.setProperty ("version", 1, nullptr);
    juce::ValueTree modulation ("modulation"), connection ("connection");
    connection.setProperty ("source", "missing-source", nullptr); connection.setProperty ("target", "missing-target", nullptr);
    modulation.addChild (connection, -1, nullptr); relationships.addChild (modulation, -1, nullptr);
    juce::ValueTree teleports ("pipeTeleports"), teleport ("teleport");
    teleport.setProperty ("id", "teleport-a", nullptr); teleport.setProperty ("destination", "missing-teleport", nullptr);
    teleports.addChild (teleport, -1, nullptr); relationships.addChild (teleports, -1, nullptr);
    const auto relationshipReport = validate (relationships);
    if (! relationshipReport.contains ("orphan modulation") || ! relationshipReport.contains ("orphan teleport")) return 6;

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
