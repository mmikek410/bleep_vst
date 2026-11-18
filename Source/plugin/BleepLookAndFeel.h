#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace bleep::ui
{

/** Embedded typefaces (SIL OFL, see Assets/Fonts). Loaded once from binary
    data and shared by every editor instance.
      display  Michroma: brand (wordmark, tagline, section titles)
      others   Barlow Condensed: everything you read while working */
struct Fonts
{
    Fonts();

    juce::Typeface::Ptr display, semiBold, medium, regular, light;

    /** Tracking is a fraction of the font height (Barlow Condensed is narrow
        and likes air). */
    juce::Font make (const juce::Typeface::Ptr& t, float height, float tracking = 0.0f) const;

    juce::Font title (float h = 10.0f) const   { return make (display, h, 0.22f); }   // section titles
    juce::Font caption (float h = 11.0f) const { return make (medium, h, 0.16f); }   // knob names
    juce::Font button (float h = 13.0f) const  { return make (medium, h, 0.14f); }
    juce::Font value (float h = 12.0f) const   { return make (regular, h, 0.04f); }   // numbers, note names
};

/** Flat, thin-lined industrial look. All text uses the embedded fonts. */
class BleepLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BleepLookAndFeel();

    /** Marks a Label as a caption (medium, tracked) instead of a value. */
    static void makeCaption (juce::Label& l) { l.getProperties().set ("bleepCaption", true); }

    /** Marks a TextButton as the primary action (amber, larger type). */
    static void makePrimary (juce::Button& b) { b.getProperties().set ("bleepPrimary", true); }

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    const Fonts& fonts() const { return *sharedFonts; }

private:
    juce::SharedResourcePointer<Fonts> sharedFonts;
};

} // namespace bleep::ui
