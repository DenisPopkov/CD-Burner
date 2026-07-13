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
        { "btn.build_sides", "Prepare" },
        { "disc.length", "Disc length" },
        { "disc.parameters", "Disc" },
        { "disc.custom", "Custom" },
        { "disc.cd74", "CD74" },
        { "disc.cd80", "CD80" },
        { "disc.tooltip.custom", "Custom total disc length" },
        { "disc.tooltip.cd74", "CD74 — 74 minutes" },
        { "disc.tooltip.cd80", "CD80 — 80 minutes" },
        { "status.choose_then_prepare", "Adjust disc length, then Prepare" },
        { "status.scanning", "Scanning %s..." },
        { "status.loading", "Loading %s..." },
        { "status.load_failed", "Load failed: %s" },
        { "status.added", "Added %s" },
        { "status.add_music_first", "Add music first" },
        { "status.preparing", "Preparing..." },
        { "status.pick_folder", "Pick a folder first" },
        { "status.preparing_discs", "Preparing %d discs..." },
        { "status.preparing_tracks", "Preparing tracks..." },
        { "status.prepared_tracks_done", "Prepared tracks saved in a folder next to your music" },
        { "btn.burn_cd", "Burn CD" },
        { "status.no_burner", "No optical drive found." },
        { "status.burning_cd", "Burning CD..." },
        { "status.burn_complete", "Audio CD burned successfully" },
        { "status.burn_failed", "CD burn failed" },
        { "status.burn_disc_done_next", "Disc %d burned. Insert the next blank CD and press Burn CD." },
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
