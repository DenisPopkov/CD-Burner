#pragma once

#include <juce_core/juce_core.h>

namespace cdb
{

class CdBurnerLocale
{
public:
    static CdBurnerLocale& instance();
    juce::String tr(const juce::String& key) const;

private:
    CdBurnerLocale() = default;
};

inline juce::String tr(const juce::String& key)
{
    return CdBurnerLocale::instance().tr(key);
}

inline juce::String trf(const juce::String& key, const juce::String& arg)
{
    return tr(key).replace("%s", arg);
}

inline juce::String trf(const juce::String& key, int value)
{
    return tr(key).replace("%d", juce::String(value));
}

}
