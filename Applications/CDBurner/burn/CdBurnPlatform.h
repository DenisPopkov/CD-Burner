#pragma once

#include "CdBurnService.h"

#if JUCE_MAC
namespace cassette::cdburn_platform
{
std::vector<CdBurnDevice> listDevices();
CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                           const juce::Array<juce::File>& trackFiles,
                           CdBurnService::ProgressCallback onProgress);
juce::String platformBurnHint();
}
#elif JUCE_WINDOWS
namespace cassette::cdburn_platform
{
std::vector<CdBurnDevice> listDevices();
CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                           const juce::Array<juce::File>& trackFiles,
                           CdBurnService::ProgressCallback onProgress);
juce::String platformBurnHint();
}
#elif JUCE_LINUX
namespace cassette::cdburn_platform
{
std::vector<CdBurnDevice> listDevices();
CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                           const juce::Array<juce::File>& trackFiles,
                           CdBurnService::ProgressCallback onProgress);
juce::String platformBurnHint();
}
#endif
