#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace cassette
{

constexpr double kRedBookSampleRate = 44100.0;

struct RedBookTrackInfo
{
    juce::File file;
    juce::String title;
    double durationSec = 0.0;
};

class RedBookExporter
{
public:
    /** Writes stereo 16-bit PCM WAV at 44.1 kHz (resamples/converts if needed). */
    static bool writeRedBookWav(const juce::File& file,
                                const juce::AudioBuffer<float>& buffer,
                                double sourceSampleRate);

    static bool isRedBookWavFile(const juce::File& file);

    /** ExactAudioCopy-style multi-file CUE (one FILE per track, INDEX 01 at 00:00:00). */
    static bool writeCueSheet(const juce::File& cueFile,
                              const juce::String& albumTitle,
                              const juce::String& artist,
                              const std::vector<RedBookTrackInfo>& tracks);

    /** Simple M3U playlist matching CD-rip layout. */
    static bool writeM3uPlaylist(const juce::File& m3uFile,
                                 const juce::String& artist,
                                 const std::vector<RedBookTrackInfo>& tracks);

    /** Wipe prior prepared WAV / CUE / M3U so re-prepare matches a clean CD folder. */
    static void clearPreparedDirectory(const juce::File& directory);

    /** Copy cover.jpg / Cover.jpg from source folder into prepared folder when present. */
    static bool copyCoverArtIfPresent(const juce::File& sourceFolder, const juce::File& preparedFolder);
};

}
