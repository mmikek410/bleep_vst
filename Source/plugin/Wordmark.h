#pragma once

#include "BleepLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace bleep::ui
{

/** The BLEEP / 001 wordmark (direction C: experimental / extended, Michroma).

    Everything that shapes it lives in `Style`, so the mark can be revised
    without touching layout code:
      BLEEP  — Michroma, wide extended capitals, light tracking, off-white
      /      — Michroma, smaller, amber: the single accent
      001    — Michroma, smaller, dim, baseline-aligned
    plus an optional small tagline after a hairline divider. */
class Wordmark final : public juce::Component
{
public:
    struct Style
    {
        float nameHeightRatio   = 0.62f;   // of component height
        float nameTracking      = 0.16f;   // fraction of font height
        float suffixScale       = 0.42f;   // "/ 001" size relative to the name
        float suffixTracking    = 0.20f;
        float suffixGap         = 0.55f;   // gap before "/", fraction of name height
        juce::String name       = "BLEEP";
        juce::String number     = "001";
        juce::String tagline    = "GENERATIVE BLEEP SYNTHESIZER";
        bool showTagline        = true;
    };

    Wordmark() { setInterceptsMouseClicks (false, false); }

    void setStyle (const Style& s) { style = s; repaint(); }
    const Style& getStyle() const { return style; }

    void paint (juce::Graphics& g) override
    {
        using namespace bleep::theme;
        const auto& f = *fonts;
        const float h = (float) getHeight();

        const auto nameFont = f.make (f.display, h * style.nameHeightRatio, style.nameTracking);
        const auto suffixFont = f.make (f.display, h * style.nameHeightRatio * style.suffixScale, style.suffixTracking);

        // Baseline shared by all parts.
        const float baseline = h * 0.5f + nameFont.getAscent() * 0.5f - nameFont.getDescent() * 0.15f;
        float x = 0.0f;

        auto draw = [&] (const juce::String& s, const juce::Font& font, juce::Colour c)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (font, s, x, baseline);
            g.setColour (c);
            ga.draw (g);
            x = ga.getBoundingBox (0, -1, true).getRight();
        };

        draw (style.name, nameFont, text);
        x += h * style.nameHeightRatio * style.suffixGap - nameFont.getHeight() * style.nameTracking;  // trim trailing tracking
        draw ("/", suffixFont, amber);
        x += suffixFont.getHeight() * 0.45f;
        draw (style.number, suffixFont, textDim);

        if (style.showTagline && x + 300.0f < (float) getWidth())
        {
            x += h * 0.5f;
            g.setColour (outline);
            g.drawVerticalLine ((int) x, h * 0.28f, h * 0.72f);
            x += h * 0.4f;
            juce::GlyphArrangement ga;
            ga.addLineOfText (f.title (8.0f), style.tagline, x, baseline - 1.0f);
            g.setColour (textDim);
            ga.draw (g);
        }
    }

private:
    Style style;
    juce::SharedResourcePointer<Fonts> fonts;
};

} // namespace bleep::ui
