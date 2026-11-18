#include "BleepLookAndFeel.h"

#include "BinaryData.h"

namespace bleep::ui
{

using namespace bleep::theme;

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
}

Fonts::Fonts()
    : display  (load (BinaryData::MichromaRegular_ttf,          BinaryData::MichromaRegular_ttfSize)),
      semiBold (load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize)),
      medium   (load (BinaryData::BarlowCondensedMedium_ttf,   BinaryData::BarlowCondensedMedium_ttfSize)),
      regular  (load (BinaryData::BarlowCondensedRegular_ttf,  BinaryData::BarlowCondensedRegular_ttfSize)),
      light    (load (BinaryData::BarlowCondensedLight_ttf,    BinaryData::BarlowCondensedLight_ttfSize))
{
}

juce::Font Fonts::make (const juce::Typeface::Ptr& t, float height, float tracking) const
{
    auto options = t != nullptr ? juce::FontOptions (t) : juce::FontOptions();  // fallback: system sans
    return juce::Font (options.withHeight (height).withKerningFactor (tracking));
}

BleepLookAndFeel::BleepLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Slider::rotarySliderFillColourId, amber);
    setColour (juce::Slider::rotarySliderOutlineColourId, outline);
    setColour (juce::Slider::thumbColourId, amber);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, amber.withAlpha (0.4f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, surfaceHi);
    setColour (juce::TextButton::buttonOnColourId, amberDim);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::ComboBox::backgroundColourId, surfaceHi);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, amber);
    setColour (juce::PopupMenu::backgroundColourId, surface);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::TooltipWindow::backgroundColourId, surfaceHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outline);
}

juce::Font BleepLookAndFeel::getLabelFont (juce::Label& l)
{
    const float h = l.getFont().getHeight();
    return l.getProperties().contains ("bleepCaption") ? fonts().caption (h) : fonts().value (h);
}

juce::Font BleepLookAndFeel::getTextButtonFont (juce::TextButton& b, int buttonHeight)
{
    if (b.getProperties().contains ("bleepPrimary"))
        return fonts().make (fonts().semiBold, juce::jmin (22.0f, (float) buttonHeight * 0.42f), 0.32f);
    return fonts().button (juce::jmin (14.0f, (float) buttonHeight * 0.5f));
}

juce::Font BleepLookAndFeel::getComboBoxFont (juce::ComboBox&)   { return fonts().button (14.0f); }
juce::Font BleepLookAndFeel::getPopupMenuFont()                    { return fonts().button (15.0f); }
juce::Font BleepLookAndFeel::getSliderPopupFont (juce::Slider&)   { return fonts().value (13.0f); }

juce::Label* BleepLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, text);
    l->setColour (juce::Label::outlineWhenEditingColourId, amber);
    l->setColour (juce::TextEditor::backgroundColourId, surfaceHi);
    l->setColour (juce::TextEditor::textColourId, text);
    l->setColour (juce::TextEditor::highlightColourId, amber.withAlpha (0.4f));
    l->setColour (juce::TextEditor::focusedOutlineColourId, amber);
    l->setFont (juce::FontOptions (13.0f));
    return l;
}

void BleepLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool primary = b.getProperties().contains ("bleepPrimary");
    const bool on = b.getToggleState();

    juce::Colour fill = primary ? amber : (on ? amberDim : surfaceHi);
    if (! b.isEnabled())   fill = fill.withMultipliedAlpha (0.4f);
    else if (down)         fill = fill.brighter (0.15f);
    else if (highlighted)  fill = fill.brighter (primary ? 0.08f : 0.06f);

    g.setColour (fill);
    g.fillRect (r);
    g.setColour (primary ? amber.brighter (0.2f) : (on ? amber : outline));
    g.drawRect (r, 1.0f);
}

void BleepLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool primary = b.getProperties().contains ("bleepPrimary");
    const bool on = b.getToggleState();
    auto colour = (primary || on) ? background : text;
    if (! b.isEnabled())
        colour = textDim.withMultipliedAlpha (0.6f);
    g.setColour (colour);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, false);
}

void BleepLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    // Thin arcs: a 1.5 px track, a 2 px amber value arc and a short hairline pointer.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = s.isEnabled() ? 1.0f : 0.4f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (outline.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // Bipolar parameters (e.g. FINE) fill from the centre.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (amber.withMultipliedAlpha (alpha));
    g.strokePath (value, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const auto inner = centre.getPointOnCircumference (radius * 0.35f, angle);
    const auto outer = centre.getPointOnCircumference (radius - 4.0f, angle);
    g.setColour (text.withMultipliedAlpha (alpha));
    g.drawLine ({ inner, outer }, 1.5f);
}

void BleepLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (r);
    g.setColour (box.hasKeyboardFocus (false) ? amber : outline);
    g.drawRect (r, 1.0f);

    // Small amber chevron on the right.
    const float cx = (float) width - 12.0f, cy = (float) height * 0.5f;
    juce::Path p;
    p.startNewSubPath (cx - 4.0f, cy - 2.0f);
    p.lineTo (cx, cy + 2.0f);
    p.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (amber);
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

void BleepLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

} // namespace bleep::ui
