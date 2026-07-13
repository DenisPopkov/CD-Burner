#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include "dsp/CassetteProfile.h"
#include "dsp/MasteringOptions.h"
#include "project/FolderMixBuilder.h"

namespace cassette
{

class DiscSetupPanel : public juce::Component,
                       private juce::Button::Listener,
                       private juce::Slider::Listener
{
public:
    std::function<void()> onPrepareClicked;
    std::function<void()> onSetupChanged;

    DiscSetupPanel();

    CassetteProfile getCassetteProfile() const;
    MasteringOptions getMasteringOptions() const;
    TapeLengthSpec getTapeLengthSpec() const;

    void setMainScreenMode(bool mainScreen);
    void setCompactToolbarMode(bool compact);
    void setMixtapeMode(bool mixtape);
    void setPrepareEnabled(bool enabled);
    void setPrepareVisible(bool visible);
    void setInteractionEnabled(bool enabled);
    void setTapeFitSummary(const juce::String& text, bool ok);

    juce::String discSummaryText() const;
    bool isCustomTapeLengthSelected() const;
    int getPreferredHeight() const;
    void triggerPrepare() { if (onPrepareClicked) onPrepareClicked(); }
    void refreshLocalisedText();

private:
    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    void notifyChanged();
    void refreshLengthSegmentStyles();
    void syncLengthFromButtons();
    void updateLengthControlPresentation();

    bool mainScreenMode = false;
    bool compactToolbarMode = false;
    bool mixtapeMode = false;
    bool interactionEnabled = true;

    juce::Label discLengthLabel { {}, "Disc length" };
    juce::Label discFitLabel;
    juce::TextButton lengthBtnCustom { "Custom" };
    juce::TextButton lengthBtnCd74 { "CD74" };
    juce::TextButton lengthBtnCd80 { "CD80" };
    juce::Slider customMinutesSlider;
    juce::TextButton prepareButton { "Prepare" };
};

}
