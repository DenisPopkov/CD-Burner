#include "CdBurnerLocale.h"
#include <unordered_map>

namespace cdb
{

namespace
{

const std::unordered_map<std::string, const char*>& strings()
{
    static const std::unordered_map<std::string, const char*> map = {
        { "btn.new", "New" },
        { "btn.prepare", "Prepare" },
        { "btn.export_wav", "Export WAV" },
        { "btn.build_sides", "Build sides" },
        { "disc.length", "Disc length" },
        { "disc.parameters", "Disc" },
        { "disc.custom", "Custom" },
        { "disc.cd74", "CD74" },
        { "disc.cd80", "CD80" },
        { "disc.tooltip.custom", "Custom total disc length (split evenly across Side A and Side B)" },
        { "disc.tooltip.cd74", "CD74 — 37 min per side (74 min total)" },
        { "disc.tooltip.cd80", "CD80 — 40 min per side (80 min total)" },
        { "status.choose_then_prepare", "Adjust disc length, then Prepare" },
        { "status.scanning", "Scanning %s..." },
        { "status.loading", "Loading %s..." },
        { "status.load_failed", "Load failed: %s" },
        { "status.added", "Added %s" },
        { "status.add_music_first", "Add music first" },
        { "status.preparing", "Preparing..." },
        { "status.pick_folder", "Pick a folder first" },
        { "status.preparing_discs", "Preparing %d discs..." },
        { "status.preparing_sides", "Preparing Side A/B..." },
        { "status.prepare_first", "Prepare first" },
        { "status.exported", "Exported %s" },
        { "status.export_failed", "Export failed" },
        { "status.unsupported_drop", "Drop a folder or audio file" },
        { "dialog.cancel", "Cancel" },
    };
    return map;
}

}

CdBurnerLocale& CdBurnerLocale::instance()
{
    static CdBurnerLocale locale;
    return locale;
}

juce::String CdBurnerLocale::tr(const juce::String& key) const
{
    const auto& map = strings();
    const auto it = map.find(key.toStdString());
    if (it != map.end())
        return juce::String(it->second);
    return key;
}

}
