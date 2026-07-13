#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

namespace cassette
{

struct CdBurnDevice
{
    juce::String id;
    juce::String displayName;
};

struct CdBurnProgress
{
    int trackIndex = 0;
    int trackCount = 0;
    double overallPercent = 0.0;
    juce::String message;
};

struct CdBurnResult
{
    bool success = false;
    juce::String error;
};

class CdBurnService
{
public:
    using ProgressCallback = std::function<void(const CdBurnProgress&)>;

    static juce::String platformBurnHint();
    static std::vector<CdBurnDevice> listDevices();
    static CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                                      const juce::Array<juce::File>& trackFiles,
                                      ProgressCallback onProgress = {});
};

}
