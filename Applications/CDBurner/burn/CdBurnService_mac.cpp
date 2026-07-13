#include "CdBurnPlatform.h"
#include "../../../Source/export/RedBookExporter.h"
#include <array>

namespace cassette::cdburn_platform
{

namespace
{

juce::String runAndCapture(const juce::StringArray& args, int& exitCode)
{
    juce::ChildProcess process;
    if (!process.start(args))
    {
        exitCode = -1;
        return {};
    }

    juce::String output;
    for (;;)
    {
        char buffer[4096];
        const int read = process.readProcessOutput(buffer, static_cast<int>(sizeof(buffer)));
        if (read > 0)
            output += juce::String::fromUTF8(buffer, static_cast<size_t>(read));

        if (!process.isRunning())
            break;

        juce::Thread::sleep(50);
    }

    char buffer[4096];
    for (;;)
    {
        const int read = process.readProcessOutput(buffer, static_cast<int>(sizeof(buffer)));
        if (read <= 0)
            break;
        output += juce::String::fromUTF8(buffer, static_cast<size_t>(read));
    }

    exitCode = process.getExitCode();
    return output;
}

std::vector<CdBurnDevice> parseDrutilList(const juce::String& text)
{
    std::vector<CdBurnDevice> devices;
    const auto lines = juce::StringArray::fromLines(text);
    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWithIgnoreCase("Vendor"))
            continue;

        const auto tokens = juce::StringArray::fromTokens(trimmed, " \t", "");
        if (tokens.size() < 2)
            continue;

        CdBurnDevice device;
        device.id = tokens[0] + "@" + tokens[1];
        device.displayName = trimmed;
        devices.push_back(std::move(device));
    }
    return devices;
}

void reportProgress(const CdBurnService::ProgressCallback& onProgress,
                    int trackIndex,
                    int trackCount,
                    double percent,
                    const juce::String& message)
{
    if (!onProgress)
        return;

    CdBurnProgress progress;
    progress.trackIndex = trackIndex;
    progress.trackCount = trackCount;
    progress.overallPercent = percent;
    progress.message = message;
    onProgress(progress);
}

} // namespace

juce::String platformBurnHint()
{
    return "Insert a blank CD-R and connect a USB or built-in optical drive.";
}

std::vector<CdBurnDevice> listDevices()
{
    int exitCode = 0;
    const auto output = runAndCapture({ "/usr/bin/drutil", "list" }, exitCode);
    auto devices = parseDrutilList(output);
    if (devices.empty())
    {
        CdBurnDevice fallback;
        fallback.id = "default";
        fallback.displayName = "Default optical drive";
        devices.push_back(std::move(fallback));
    }
    return devices;
}

CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                           const juce::Array<juce::File>& trackFiles,
                           CdBurnService::ProgressCallback onProgress)
{
    if (trackFiles.isEmpty())
        return { false, "No prepared tracks to burn." };

    for (const auto& track : trackFiles)
    {
        if (!RedBookExporter::isRedBookWavFile(track))
            return { false, "Prepared tracks must be 16-bit / 44.1 kHz WAV: " + track.getFileName() };
    }

    const auto burnFolder = trackFiles.getFirst().getParentDirectory();
    juce::StringArray args { "/usr/bin/drutil" };
    if (device.id.isNotEmpty() && device.id != "default")
    {
        args.add("-drive");
        args.add(device.id);
    }
    args.add("burn");
    args.add("-audio");
    args.add("-noverify");
    args.add(burnFolder.getFullPathName());

    reportProgress(onProgress, 0, trackFiles.size(), 0.02, "Starting audio CD burn...");

    juce::ChildProcess process;
    if (!process.start(args))
        return { false, "Failed to start drutil burn." };

    juce::String output;
    while (process.isRunning())
    {
        char buffer[4096];
        const int read = process.readProcessOutput(buffer, static_cast<int>(sizeof(buffer)));
        if (read > 0)
            output += juce::String::fromUTF8(buffer, static_cast<size_t>(read));

        reportProgress(onProgress, 1, trackFiles.size(), 0.5, "Burning audio CD...");
        juce::Thread::sleep(100);
    }

    char buffer[4096];
    for (;;)
    {
        const int read = process.readProcessOutput(buffer, static_cast<int>(sizeof(buffer)));
        if (read <= 0)
            break;
        output += juce::String::fromUTF8(buffer, static_cast<size_t>(read));
    }

    const int exitCode = process.getExitCode();
    if (exitCode != 0)
    {
        const auto tail = output.trim().isNotEmpty()
                              ? output.trim().substring(juce::jmax(0, output.length() - 240))
                              : juce::String("drutil failed");
        return { false, tail };
    }

    reportProgress(onProgress, trackFiles.size(), trackFiles.size(), 1.0, "Burn complete");
    return { true, {} };
}

}
