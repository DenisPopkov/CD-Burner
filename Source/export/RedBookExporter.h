#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

constexpr double kRedBookSampleRate = 44100.0;

class RedBookExporter
{
public:
    /** Writes stereo 16-bit PCM WAV at 44.1 kHz (resamples/converts if needed). */
    static bool writeRedBookWav(const juce::File& file,
                                const juce::AudioBuffer<float>& buffer,
                                double sourceSampleRate);

    static bool isRedBookWavFile(const juce::File& file);
};

}
