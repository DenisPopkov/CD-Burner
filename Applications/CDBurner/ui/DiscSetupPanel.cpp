#include "DiscSetupPanel.h"
#include "UiTheme.h"
#include "../locale/CdBurnerLocale.h"

namespace cassette
{

namespace
{

TapeLengthSpec cdLengthSpec(const juce::String& label, double totalMinutes)
{
    TapeLengthSpec spec;
    spec.label = label;
    spec.minutesPerSide = totalMinutes * 0.5;
    return spec;
}

}

DiscSetupPanel::DiscSetupPanel()
{
    using namespace ui;

    addAndMakeVisible(discLengthLabel);
    Theme::applyLabel(discLengthLabel, Theme::captionFont(), Theme::textSecondary());
    discLengthLabel.setVisible(false);

    addAndMakeVisible(discFitLabel);
    Theme::applyLabel(discFitLabel, Theme::captionFont(), Theme::textSecondary());
    discFitLabel.setVisible(false);

    for (auto* b : { &lengthBtnCustom, &lengthBtnCd74, &lengthBtnCd80 })
    {
        addAndMakeVisible(*b);
        b->setRadioGroupId(9200);
        b->setClickingTogglesState(true);
        b->setComponentID("tape-type-segment");
        b->addListener(this);
        b->setVisible(false);
    }
    lengthBtnCd80.setToggleState(true, juce::dontSendNotification);
    refreshLengthSegmentStyles();

    addAndMakeVisible(customMinutesSlider);
    customMinutesSlider.setRange(20.0, 160.0, 2.0);
    customMinutesSlider.setValue(80.0, juce::dontSendNotification);
    customMinutesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    customMinutesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 24);
    customMinutesSlider.textFromValueFunction = [](double value) {
        return juce::String(juce::roundToInt(value)) + " min";
    };
    Theme::styleAccentSlider(customMinutesSlider);
    customMinutesSlider.setVisible(false);
    customMinutesSlider.setEnabled(false);
    customMinutesSlider.onValueChange = [this] { notifyChanged(); };
    customMinutesSlider.addListener(this);

    addAndMakeVisible(prepareButton);
    Theme::styleAccentButton(prepareButton);
    prepareButton.addListener(this);
    prepareButton.setEnabled(false);
    prepareButton.setVisible(false);

    refreshLocalisedText();
}

void DiscSetupPanel::refreshLocalisedText()
{
    lengthBtnCustom.setButtonText(cdb::tr("disc.custom"));
    lengthBtnCd74.setButtonText(cdb::tr("disc.cd74"));
    lengthBtnCd80.setButtonText(cdb::tr("disc.cd80"));
    lengthBtnCustom.setTooltip({});
    lengthBtnCd74.setTooltip({});
    lengthBtnCd80.setTooltip({});
    discLengthLabel.setText(cdb::tr("disc.length"), juce::dontSendNotification);
    if (!mainScreenMode)
        prepareButton.setButtonText(mixtapeMode ? cdb::tr("btn.build_sides") : cdb::tr("btn.prepare"));
    refreshLengthSegmentStyles();
    repaint();
}

CassetteProfile DiscSetupPanel::getCassetteProfile() const
{
    return CassetteProfile::forCdDigital();
}

MasteringOptions DiscSetupPanel::getMasteringOptions() const
{
    MasteringOptions options;
    options.maximumDigital = true;
    options.enableTruePeakLimiter = true;
    options.enableStereoTightening = false;
    options.perceptualAutoFallback = true;
    return options;
}

TapeLengthSpec DiscSetupPanel::getTapeLengthSpec() const
{
    if (lengthBtnCustom.getToggleState())
        return cdLengthSpec("Custom", customMinutesSlider.getValue());
    if (lengthBtnCd74.getToggleState())
        return cdLengthSpec("CD74", 74.0);
    return cdLengthSpec("CD80", 80.0);
}

bool DiscSetupPanel::isCustomTapeLengthSelected() const
{
    return lengthBtnCustom.getToggleState();
}

int DiscSetupPanel::getPreferredHeight() const
{
    if (!mainScreenMode || compactToolbarMode || !mixtapeMode)
        return 0;

    // Keep segment row at full 32px always; only grow for the custom slider.
    constexpr int padY = 10;
    constexpr int labelH = 18;
    constexpr int labelGap = 6;
    constexpr int segH = 32;
    constexpr int sliderGap = 6;
    constexpr int sliderH = 28;

    int h = padY * 2 + labelH + labelGap + segH;
    if (isCustomTapeLengthSelected())
        h += sliderGap + sliderH;
    return h;
}

void DiscSetupPanel::setMainScreenMode(bool mainScreen)
{
    mainScreenMode = mainScreen;
    prepareButton.setVisible(!mainScreen);
    discLengthLabel.setVisible(!mainScreen && mixtapeMode);
    resized();
    repaint();
}

void DiscSetupPanel::setCompactToolbarMode(bool compact)
{
    compactToolbarMode = compact;
    resized();
    repaint();
}

void DiscSetupPanel::setMixtapeMode(bool mixtape)
{
    mixtapeMode = mixtape;
    const bool showLength = mainScreenMode && mixtape;
    lengthBtnCustom.setVisible(showLength);
    lengthBtnCd74.setVisible(showLength);
    lengthBtnCd80.setVisible(showLength);
    discLengthLabel.setVisible(!mainScreenMode && mixtape);
    discFitLabel.setVisible(false);
    updateLengthControlPresentation();
    if (!mainScreenMode)
        prepareButton.setButtonText(mixtape ? cdb::tr("btn.build_sides") : cdb::tr("btn.prepare"));
    if (!mixtape)
        discFitLabel.setText({}, juce::dontSendNotification);
    resized();
    repaint();
}

void DiscSetupPanel::setTapeFitSummary(const juce::String& text, bool ok)
{
    if (mainScreenMode)
    {
        discFitLabel.setText({}, juce::dontSendNotification);
        discFitLabel.setVisible(false);
        return;
    }

    discFitLabel.setText(text, juce::dontSendNotification);
    discFitLabel.setColour(juce::Label::textColourId,
                           ok ? ui::Theme::textPrimary() : ui::Theme::warnAmber());
    resized();
    repaint();
}

void DiscSetupPanel::setPrepareEnabled(bool enabled)
{
    prepareButton.setEnabled(enabled);
}

void DiscSetupPanel::setPrepareVisible(bool visible)
{
    prepareButton.setVisible(visible);
    resized();
}

void DiscSetupPanel::setInteractionEnabled(bool enabled)
{
    interactionEnabled = enabled;
    lengthBtnCustom.setEnabled(enabled);
    lengthBtnCd74.setEnabled(enabled);
    lengthBtnCd80.setEnabled(enabled);
    updateLengthControlPresentation();
    refreshLengthSegmentStyles();
}

juce::String DiscSetupPanel::discSummaryText() const
{
    return getCassetteProfile().displayName + " / " + getTapeLengthSpec().label;
}

void DiscSetupPanel::refreshLengthSegmentStyles()
{
    const bool enabled = interactionEnabled && isEnabled();
    ui::Theme::styleTapeTypeSegment(lengthBtnCustom, lengthBtnCustom.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(lengthBtnCd74, lengthBtnCd74.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(lengthBtnCd80, lengthBtnCd80.getToggleState(), enabled);
}

void DiscSetupPanel::syncLengthFromButtons()
{
    refreshLengthSegmentStyles();
}

void DiscSetupPanel::updateLengthControlPresentation()
{
    const bool customSelected = isCustomTapeLengthSelected();
    customMinutesSlider.setVisible(mixtapeMode && customSelected && mainScreenMode);
    customMinutesSlider.setEnabled(interactionEnabled && customSelected);
    ui::Theme::styleAccentSlider(customMinutesSlider);
    resized();
}

void DiscSetupPanel::notifyChanged()
{
    if (onSetupChanged)
        onSetupChanged();
}

void DiscSetupPanel::paint(juce::Graphics& g)
{
    if (!mainScreenMode)
    {
        ui::Theme::drawCard(g, getLocalBounds(), "Disc Setup");
        return;
    }

    if (compactToolbarMode || !mixtapeMode)
        return;

    auto header = getLocalBounds().reduced(14, 10);
    ui::Theme::drawSectionLabel(g, header.removeFromTop(18), cdb::tr("disc.length"));
}

void DiscSetupPanel::resized()
{
    auto r = getLocalBounds();

    if (mainScreenMode)
    {
        if (compactToolbarMode || !mixtapeMode)
            return;

        r = r.reduced(14, 10);
        r.removeFromTop(18);
        r.removeFromTop(6);

        auto lengthRow = r.removeFromTop(32);
        const int lenGap = 4;
        const int lenSegW = (lengthRow.getWidth() - lenGap * 2) / 3;
        lengthBtnCd74.setBounds(lengthRow.removeFromLeft(lenSegW));
        lengthRow.removeFromLeft(lenGap);
        lengthBtnCd80.setBounds(lengthRow.removeFromLeft(lenSegW));
        lengthRow.removeFromLeft(lenGap);
        lengthBtnCustom.setBounds(lengthRow);

        if (lengthBtnCustom.getToggleState())
        {
            r.removeFromTop(6);
            customMinutesSlider.setBounds(r.removeFromTop(28));
        }
        return;
    }

    r = r.reduced(14).withTrimmedTop(32);
    if (prepareButton.isVisible())
        prepareButton.setBounds(r.removeFromTop(36));
}

void DiscSetupPanel::buttonClicked(juce::Button* b)
{
    if (b == &prepareButton && onPrepareClicked)
    {
        onPrepareClicked();
        return;
    }

    if (b == &lengthBtnCustom || b == &lengthBtnCd74 || b == &lengthBtnCd80)
    {
        if (b == &lengthBtnCd74)
            customMinutesSlider.setValue(74.0, juce::dontSendNotification);
        else if (b == &lengthBtnCd80)
            customMinutesSlider.setValue(80.0, juce::dontSendNotification);

        syncLengthFromButtons();
        updateLengthControlPresentation();
        notifyChanged();
    }
}

void DiscSetupPanel::sliderValueChanged(juce::Slider*)
{
    notifyChanged();
}

}
