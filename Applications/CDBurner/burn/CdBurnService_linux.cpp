#include "CdBurnPlatform.h"
#include "../../../Source/export/RedBookExporter.h"

namespace cassette::cdburn_platform
{

namespace
{

juce::File findWodimExecutable()
{
    const juce::StringArray candidates {
        "/usr/bin/wodim",
        "/usr/bin/cdrecord",
        "/bin/wodim",
        "/bin/cdrecord",
    };

    for (const auto& path : candidates)
    {
        const juce::File file(path);
        if (file.existsAsFile())
            return file;
    }

    return {};
}

juce::String runAndCapture(const juce::String& executable, const juce::StringArray& args, int& exitCode)
{
    juce::ChildProcess process;
    juce::StringArray command;
    command.add(executable);
    command.addArray(args);

    if (!process.start(command))
    {
        exitCode = -1;
        return {};
    }

    juce::String output;
    while (process.isRunning() || process.readProcessOutputWithoutBlocking() > 0)
    {
        char buffer[4096];
        const int read = process.readProcessOutput(buffer, static_cast<int>(sizeof(buffer)));
        if (read > 0)
            output += juce::String::fromUTF8(buffer, static_cast<size_t>(read));
        else
            juce::Thread::sleep(50);
    }

    exitCode = process.getExitCode();
    return output;
}

std::vector<CdBurnDevice> parseScanbus(const juce::String& text)
{
    std::vector<CdBurnDevice> devices;
    const auto lines = juce::StringArray::fromLines(text);
    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();
        if (!trimmed.contains("CD-ROM") && !trimmed.contains("CD-R") && !trimmed.contains("DVD"))
            continue;

        const int devIndex = trimmed.indexOf("dev=");
        if (devIndex < 0)
            continue;

        CdBurnDevice device;
        device.id = trimmed.substring(devIndex + 4).trim().upToFirstOccurrenceOf(" ", false, false);
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
    return "Install wodim (cdrtools) and ensure your user can access the optical drive (/dev/sr0).";
}

std::vector<CdBurnDevice> listDevices()
{
    const auto wodim = findWodimExecutable();
    if (!wodim.existsAsFile())
        return {};

    int exitCode = 0;
    const auto output = runAndCapture(wodim.getFullPathName(), { "-scanbus" }, exitCode);
    auto devices = parseScanbus(output);
    if (devices.empty())
    {
        CdBurnDevice fallback;
        fallback.id = "/dev/sr0";
        fallback.displayName = "/dev/sr0";
        devices.push_back(std::move(fallback));
    }
    return devices;
}

CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                           const juce::Array<juce::File>& trackFiles,
                           CdBurnService::ProgressCallback onProgress)
{
    const auto wodim = findWodimExecutable();
    if (!wodim.existsAsFile())
        return { false, "wodim not found. Install cdrtools / wodim package." };

    if (trackFiles.isEmpty())
        return { false, "No prepared tracks to burn." };

    for (const auto& track : trackFiles)
    {
        if (!RedBookExporter::isRedBookWavFile(track))
            return { false, "Prepared tracks must be 16-bit / 44.1 kHz WAV: " + track.getFileName() };
    }

    juce::StringArray args;
    args.add("-v");
    args.add("-dao");
    args.add("-audio");
    args.add("-pad");
    args.add("dev=" + (device.id.isNotEmpty() ? device.id : juce::String("/dev/sr0")));
    for (const auto& track : trackFiles)
        args.add(track.getFullPathName());

    reportProgress(onProgress, 0, trackFiles.size(), 0.02, "Starting audio CD burn...");

    juce::ChildProcess process;
    juce::StringArray command;
    command.add(wodim.getFullPathName());
    command.addArray(args);
    if (!process.start(command))
        return { false, "Failed to start wodim." };

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
                              : juce::String("wodim failed");
        return { false, tail };
    }

    reportProgress(onProgress, trackFiles.size(), trackFiles.size(), 1.0, "Burn complete");
    return { true, {} };
}

}
