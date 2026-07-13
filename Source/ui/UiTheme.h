#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette::ui
{

struct Theme
{
    struct Palette
    {
        juce::Colour background { 0xfff5f0e6 };
        juce::Colour panel { 0xfffaf7f0 };
        juce::Colour card { 0xffffffff };
        juce::Colour border { 0xff111111 };
        juce::Colour borderLight { 0xffcccccc };
        juce::Colour accent { 0xffe85500 };
        juce::Colour accentMuted { 0xffff7a33 };
        juce::Colour exportGreen { 0xff2d6a4f };
        juce::Colour okGreen { 0xff3d8b5f };
        juce::Colour warnAmber { 0xffc45c00 };
        juce::Colour failRed { 0xffc0392b };
        juce::Colour textPrimary { 0xff111111 };
        juce::Colour textSecondary { 0xff444444 };
        juce::Colour textMuted { 0xff666666 };
        juce::Colour trackBlue { 0xff4a90d9 };
        juce::Colour trackGreen { 0xff5cb88a };
        juce::Colour trackGrey { 0xff9aa0a6 };
        juce::Colour sidebarHighlight { 0xffebe4d8 };
        juce::Colour buttonDisabledFill { 0xffe8e3d9 };
        juce::Colour buttonDisabledText { 0xff4a4a4a };
        juce::Colour buttonDisabledBorder { 0xffb8b2a8 };
        /** Dark chrome (sidebar / icon plate). Deck keeps this equal to background. */
        juce::Colour chrome { 0xfff5f0e6 };
        juce::Colour chromeText { 0xff111111 };
        juce::Colour chromeMuted { 0xff666666 };
    };

    static Palette& palette()
    {
        static Palette p;
        return p;
    }

    /**
     * Graphite chrome (#424149) from the icon plate + cool iridescent accent
     * sampled from the CD rainbow (violet-blue), not Deck orange.
     */
    static void applyCdBurnerPalette()
    {
        auto& p = palette();
        p.background = juce::Colour(0xfff2f1f6);
        p.panel = juce::Colour(0xfff7f6fb);
        p.card = juce::Colour(0xffffffff);
        p.border = juce::Colour(0xff2a292e);
        p.borderLight = juce::Colour(0xffd2d0da);
        // Iridescent violet-blue from the disc prism — CTA / selection / wizard.
        p.accent = juce::Colour(0xff5b6ba8);
        p.accentMuted = juce::Colour(0xff7a87b8);
        p.exportGreen = juce::Colour(0xff3d7a6a); // cooler mint success, also on-disc
        p.okGreen = juce::Colour(0xff458a78);
        p.warnAmber = juce::Colour(0xff9a7a45); // muted gold from disc highlight
        p.failRed = juce::Colour(0xffb04a52);
        p.textPrimary = juce::Colour(0xff1a191e);
        p.textSecondary = juce::Colour(0xff5a5860);
        p.textMuted = juce::Colour(0xff7a7882);
        p.trackBlue = juce::Colour(0xff5b6ba8);
        p.trackGreen = juce::Colour(0xff458a78);
        p.trackGrey = juce::Colour(0xff9aa0a6);
        p.sidebarHighlight = juce::Colour(0xffe6e4ef);
        p.buttonDisabledFill = juce::Colour(0xffe4e2e8);
        p.buttonDisabledText = juce::Colour(0xff6a6870);
        p.buttonDisabledBorder = juce::Colour(0xffb4b2ba);
        // Soft cool rail — related to #424149 but not a dark slab.
        p.chrome = juce::Colour(0xffe4e2eb);
        p.chromeText = juce::Colour(0xff1a191e);
        p.chromeMuted = juce::Colour(0xff7a7882);
    }

    static juce::Colour background() { return palette().background; }
    static juce::Colour panel() { return palette().panel; }
    static juce::Colour card() { return palette().card; }
    static juce::Colour border() { return palette().border; }
    static juce::Colour borderLight() { return palette().borderLight; }
    static juce::Colour accent() { return palette().accent; }
    static juce::Colour accentMuted() { return palette().accentMuted; }
    static juce::Colour prepare() { return accent(); }
    static juce::Colour exportGreen() { return palette().exportGreen; }
    static juce::Colour okGreen() { return palette().okGreen; }
    static juce::Colour warnAmber() { return palette().warnAmber; }
    static juce::Colour failRed() { return palette().failRed; }
    static juce::Colour textPrimary() { return palette().textPrimary; }
    static juce::Colour textSecondary() { return palette().textSecondary; }
    static juce::Colour textMuted() { return palette().textMuted; }
    static juce::Colour trackBlue() { return palette().trackBlue; }
    static juce::Colour trackGreen() { return palette().trackGreen; }
    static juce::Colour trackGrey() { return palette().trackGrey; }
    static juce::Colour sidebarHighlight() { return palette().sidebarHighlight; }
    static juce::Colour buttonDisabledFill() { return palette().buttonDisabledFill; }
    static juce::Colour buttonDisabledText() { return palette().buttonDisabledText; }
    static juce::Colour buttonDisabledBorder() { return palette().buttonDisabledBorder; }
    static juce::Colour chrome() { return palette().chrome; }
    static juce::Colour chromeText() { return palette().chromeText; }
    static juce::Colour chromeMuted() { return palette().chromeMuted; }

    static inline juce::Colour cardBorder() { return borderLight(); }

    static juce::Font uiFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain)
    {
#if JUCE_MAC
        return juce::Font(juce::FontOptions("SF Pro Text", height, style));
#else
        return juce::Font(juce::FontOptions(height, style));
#endif
    }

    static juce::Font monoFont(float height)
    {
#if JUCE_MAC
        return juce::Font(juce::FontOptions("SF Mono", height, juce::Font::plain));
#else
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
#endif
    }

    static juce::Font titleFont() { return uiFont(16.0f, juce::Font::bold); }
    static juce::Font sectionFont() { return uiFont(11.5f, juce::Font::bold); }
    static juce::Font bodyFont() { return uiFont(13.0f); }
    static juce::Font buttonFont() { return uiFont(13.0f); }
    static juce::Font buttonFontSmall() { return uiFont(11.0f); }
    static juce::Font captionFont() { return uiFont(11.0f); }
    static juce::Font captionFontSemibold()
    {
#if JUCE_MAC
        return juce::Font(juce::FontOptions("SF Pro Text", "Semibold", 11.0f));
#else
        return uiFont(11.0f, juce::Font::bold);
#endif
    }
    static juce::Font metricFont() { return monoFont(12.0f); }
    static juce::Font scoreFont() { return uiFont(22.0f, juce::Font::bold); }

    static void applyLabel(juce::Label& label,
                           const juce::Font& font,
                           juce::Colour colour,
                           juce::Justification justification = juce::Justification::centredLeft)
    {
        label.setFont(font);
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(justification);
        label.setMinimumHorizontalScale(1.0f);
    }

    static void styleBlackButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, border());
        b.setColour(juce::TextButton::buttonOnColourId, border().brighter(0.15f));
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    static void styleNeutralButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, card());
        b.setColour(juce::TextButton::buttonOnColourId, sidebarHighlight());
        b.setColour(juce::TextButton::textColourOffId, textPrimary());
        b.setColour(juce::TextButton::textColourOnId, textPrimary());
    }

    /** Neutral control sitting on chrome() sidebar. */
    static void styleChromeNeutralButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, chrome().brighter(0.14f));
        b.setColour(juce::TextButton::buttonOnColourId, chrome().brighter(0.22f));
        b.setColour(juce::TextButton::textColourOffId, chromeText());
        b.setColour(juce::TextButton::textColourOnId, chromeText());
    }

    /** Emphasized control on chrome() sidebar (e.g. New when session active). */
    static void styleChromeActiveButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::white);
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff3f2f5));
        b.setColour(juce::TextButton::textColourOffId, chrome());
        b.setColour(juce::TextButton::textColourOnId, chrome());
    }

    static void styleAccentButton(juce::TextButton& b) { styleRecButton(b); }

    static void styleRecButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, accent());
        b.setColour(juce::TextButton::buttonOnColourId, accent().darker(0.12f));
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    static void styleExportButton(juce::TextButton& b)
    {
        styleRecButton(b);
    }

    enum class TransportButtonStyle { Neutral, Black, Rec, Export };

    static void applyTransportButtonStyle(juce::TextButton& b, TransportButtonStyle style, bool enabled)
    {
        if (!enabled)
        {
            b.setColour(juce::TextButton::buttonColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::buttonOnColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::textColourOffId, buttonDisabledText());
            b.setColour(juce::TextButton::textColourOnId, buttonDisabledText());
            return;
        }

        switch (style)
        {
            case TransportButtonStyle::Black: styleBlackButton(b); break;
            case TransportButtonStyle::Rec: styleRecButton(b); break;
            case TransportButtonStyle::Export: styleExportButton(b); break;
            case TransportButtonStyle::Neutral:
            default: styleNeutralButton(b); break;
        }
    }

    static void styleTapeTypeSegment(juce::TextButton& b, bool selected, bool enabled = true)
    {
        if (!enabled)
        {
            b.setColour(juce::TextButton::buttonColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::buttonOnColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::textColourOffId, buttonDisabledText());
            b.setColour(juce::TextButton::textColourOnId, buttonDisabledText());
            return;
        }

        b.setColour(juce::TextButton::buttonColourId, selected ? accent() : card());
        b.setColour(juce::TextButton::buttonOnColourId, accent());
        b.setColour(juce::TextButton::textColourOffId, selected ? juce::Colours::white : textPrimary());
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    static void styleSegmentButton(juce::TextButton& b, bool selected)
    {
        styleTapeTypeSegment(b, selected);
    }

    static void styleCombo(juce::ComboBox& box)
    {
        box.setColour(juce::ComboBox::backgroundColourId, card());
        box.setColour(juce::ComboBox::textColourId, textPrimary());
        box.setColour(juce::ComboBox::outlineColourId, border());
        box.setColour(juce::ComboBox::arrowColourId, textSecondary());
        box.setColour(juce::PopupMenu::backgroundColourId, card());
        box.setColour(juce::PopupMenu::textColourId, textPrimary());
        box.setColour(juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha(0.14f));
        box.setColour(juce::PopupMenu::highlightedTextColourId, accent());
    }

    static void styleAccentSlider(juce::Slider& slider)
    {
        slider.setColour(juce::Slider::backgroundColourId, sidebarHighlight());
        slider.setColour(juce::Slider::trackColourId, accent());
        slider.setColour(juce::Slider::thumbColourId, accent().darker(0.15f));
        slider.setColour(juce::Slider::textBoxTextColourId, textPrimary());
        slider.setColour(juce::Slider::textBoxBackgroundColourId, card());
        slider.setColour(juce::Slider::textBoxOutlineColourId, border());
        slider.setColour(juce::Slider::textBoxHighlightColourId, card());
    }

    static void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds, bool fill = true)
    {
        if (fill)
        {
            g.setColour(card());
            g.fillRect(bounds);
        }
        g.setColour(border());
        g.drawRect(bounds, 1);
    }

    static void drawCard(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        drawPanel(g, bounds);

        if (title.isNotEmpty())
        {
            g.setColour(textPrimary());
            g.setFont(sectionFont());
            g.drawText(title, bounds.reduced(12, 10).removeFromTop(20), juce::Justification::centredLeft);
        }
    }

    static void drawSectionLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour(textSecondary());
        g.setFont(sectionFont());
        g.drawText(text, area, juce::Justification::centredLeft);
    }

    static void drawCentredText(juce::Graphics& g,
                                const juce::String& text,
                                juce::Rectangle<int> area,
                                const juce::Font& font,
                                juce::Colour colour)
    {
        g.setColour(colour);
        g.setFont(font);
        g.drawText(text, area, juce::Justification::centred);
    }
};

}
