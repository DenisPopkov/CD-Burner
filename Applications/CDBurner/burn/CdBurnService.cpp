#include "CdBurnService.h"
#include "CdBurnPlatform.h"

namespace cassette
{

juce::String CdBurnService::platformBurnHint()
{
#if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX
    return cdburn_platform::platformBurnHint();
#else
    return "CD burning is not supported on this platform.";
#endif
}

std::vector<CdBurnDevice> CdBurnService::listDevices()
{
#if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX
    return cdburn_platform::listDevices();
#else
    return {};
#endif
}

CdBurnResult CdBurnService::burnAudioDisc(const CdBurnDevice& device,
                                          const juce::Array<juce::File>& trackFiles,
                                          ProgressCallback onProgress)
{
#if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX
    return cdburn_platform::burnAudioDisc(device, trackFiles, std::move(onProgress));
#else
    juce::ignoreUnused(device, trackFiles, onProgress);
    return { false, "CD burning is not supported on this platform." };
#endif
}

}
