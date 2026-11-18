#pragma once

#include <juce_graphics/juce_graphics.h>

namespace bleep::theme
{
    inline const juce::Colour background { 0xff0b0b0c };   // near-black
    inline const juce::Colour surface    { 0xff141416 };   // panels
    inline const juce::Colour surfaceHi  { 0xff1e1e21 };   // controls
    inline const juce::Colour outline    { 0xff2c2c31 };   // hairlines
    inline const juce::Colour text       { 0xffdcd8cf };   // warm off-white
    inline const juce::Colour textDim    { 0xff7d7a72 };
    inline const juce::Colour amber      { 0xffe08a2c };   // the one accent
    inline const juce::Colour amberDim   { 0xff8a5a24 };

    // Spacing scale (px at 100 %): everything snaps to these.
    inline constexpr int gutter  = 12;
    inline constexpr int padding = 12;
    inline constexpr int header  = 64;
}
