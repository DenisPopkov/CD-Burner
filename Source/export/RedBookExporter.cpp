#include "RedBookExporter.h"
#include "../io/AudioResampler.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

namespace cassette
{

namespace
{

juce::AudioBuffer<float> toStereo(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() >= 2)
        return buffer;

    juce::AudioBuffer<float> stereo(2, buffer.getNumSamples());
    stereo.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
    stereo.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
    return stereo;
}

juce::String cueQuoted(const juce::String& text)
{
    return "\"" + text.replaceCharacter('"', '\'') + "\"";
}

} // namespace

bool RedBookExporter::writeRedBookWav(const juce::File& file,
                                      const juce::AudioBuffer<float>& buffer,
                                      double sourceSampleRate)
{
    if (buffer.getNumSamples() <= 0 || sourceSampleRate <= 0.0)
        return false;

    auto stereo = toStereo(buffer);
    if (std::abs(sourceSampleRate - kRedBookSampleRate) > 1.0)
        resampleBufferLinear(stereo, sourceSampleRate, kRedBookSampleRate);

    for (int ch = 0; ch < stereo.getNumChannels(); ++ch)
    {
        auto* data = stereo.getWritePointer(ch);
        for (int i = 0; i < stereo.getNumSamples(); ++i)
            data[i] = juce::jlimit(-1.0f, 1.0f, data[i]);
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
    if (out == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(out.release(), kRedBookSampleRate, 2, 16, {}, 0));
    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer(stereo, 0, stereo.getNumSamples());
}

bool RedBookExporter::isRedBookWavFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    return reader->numChannels == 2
           && reader->bitsPerSample == 16
           && std::abs(reader->sampleRate - kRedBookSampleRate) < 1.0;
}

bool RedBookExporter::writeCueSheet(const juce::File& cueFile,
                                    const juce::String& albumTitle,
                                    const juce::String& artist,
                                    const std::vector<RedBookTrackInfo>& tracks)
{
    if (tracks.empty())
        return false;

    juce::FileOutputStream out(cueFile);
    if (!out.openedOk())
        return false;

    out.setNewLineString("\r\n");
    if (artist.isNotEmpty())
        out << "PERFORMER " << cueQuoted(artist) << "\r\n";
    if (albumTitle.isNotEmpty())
        out << "TITLE " << cueQuoted(albumTitle) << "\r\n";

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto& track = tracks[i];
        const auto index = static_cast<int>(i) + 1;
        out << "FILE " << cueQuoted(track.file.getFileName()) << " WAVE\r\n";
        out << "  TRACK " << juce::String(index).paddedLeft('0', 2) << " AUDIO\r\n";
        out << "    TITLE " << cueQuoted(track.title) << "\r\n";
        if (artist.isNotEmpty())
            out << "    PERFORMER " << cueQuoted(artist) << "\r\n";
        out << "    INDEX 01 00:00:00\r\n";
    }

    return !out.getStatus().failed();
}

bool RedBookExporter::writeM3uPlaylist(const juce::File& m3uFile,
                                       const juce::String& artist,
                                       const std::vector<RedBookTrackInfo>& tracks)
{
    if (tracks.empty())
        return false;

    juce::FileOutputStream out(m3uFile);
    if (!out.openedOk())
        return false;

    out.setNewLineString("\r\n");
    out << "#EXTM3U\r\n";
    for (const auto& track : tracks)
    {
        const int seconds = juce::jmax(1, static_cast<int>(std::lround(track.durationSec)));
        const auto display = artist.isNotEmpty() ? artist + " - " + track.title : track.title;
        out << "#EXTINF:" << seconds << "," << display << "\r\n";
        out << track.file.getFileName() << "\r\n";
    }

    return !out.getStatus().failed();
}

void RedBookExporter::clearPreparedDirectory(const juce::File& directory)
{
    if (!directory.isDirectory())
        return;

    for (const auto& file : directory.findChildFiles(juce::File::findFiles, false))
    {
        const auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".cue" || ext == ".m3u" || ext == ".m3u8"
            || file.getFileName().equalsIgnoreCase("cover.jpg")
            || file.getFileName().equalsIgnoreCase("cover.jpeg")
            || file.getFileName().equalsIgnoreCase("cover.png")
            || file.getFileName().equalsIgnoreCase("folder.jpg"))
        {
            file.deleteFile();
        }
    }
}

bool RedBookExporter::copyCoverArtIfPresent(const juce::File& sourceFolder, const juce::File& preparedFolder)
{
    if (!sourceFolder.isDirectory() || !preparedFolder.isDirectory())
        return false;

    static const char* names[] = {
        "cover.jpg", "Cover.jpg", "cover.jpeg", "Cover.jpeg", "cover.png", "folder.jpg", "Folder.jpg"
    };

    for (const auto* name : names)
    {
        const auto src = sourceFolder.getChildFile(name);
        if (!src.existsAsFile())
            continue;

        const auto dest = preparedFolder.getChildFile("cover.jpg");
        return src.copyFileTo(dest);
    }
    return false;
}

}
