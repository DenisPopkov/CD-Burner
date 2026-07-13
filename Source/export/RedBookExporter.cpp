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

juce::AudioBuffer<int16_t> floatToInt16(const juce::AudioBuffer<float>& stereo)
{
    juce::AudioBuffer<int16_t> out(2, stereo.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* dst = out.getWritePointer(ch);
        const auto* src = stereo.getReadPointer(ch);
        for (int i = 0; i < stereo.getNumSamples(); ++i)
            dst[i] = static_cast<int16_t>(juce::jlimit(-32768, 32767, static_cast<int>(std::lround(src[i] * 32767.0f))));
    }
    return out;
}

}

bool RedBookExporter::writeRedBookWav(const juce::File& file,
                                      const juce::AudioBuffer<float>& buffer,
                                      double sourceSampleRate)
{
    if (buffer.getNumSamples() <= 0 || sourceSampleRate <= 0.0)
        return false;

    auto stereo = toStereo(buffer);
    if (std::abs(sourceSampleRate - kRedBookSampleRate) > 1.0)
        resampleBufferLinear(stereo, sourceSampleRate, kRedBookSampleRate);

    const auto pcm = floatToInt16(stereo);

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
    if (out == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(out.release(), kRedBookSampleRate, 2, 16, {}, 0));
    if (writer == nullptr)
        return false;

    const int* channelPointers[2] = {
        reinterpret_cast<const int*>(pcm.getReadPointer(0)),
        reinterpret_cast<const int*>(pcm.getReadPointer(1)),
    };
    return writer->write(channelPointers, pcm.getNumSamples());
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

}
