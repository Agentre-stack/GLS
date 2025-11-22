#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "GoodluckLogoData.h"

namespace gls::ui
{
struct Colours
{
    static juce::Colour background() noexcept      { return juce::Colour::fromRGB (8, 9, 11); }
    static juce::Colour panel() noexcept           { return juce::Colour::fromRGB (17, 19, 24); }
    static juce::Colour outline() noexcept         { return juce::Colour::fromRGB (39, 42, 50); }
    static juce::Colour grid() noexcept            { return juce::Colour::fromRGB (30, 32, 39); }
    static juce::Colour text() noexcept            { return juce::Colour::fromRGB (255, 255, 255); }
    static juce::Colour textSecondary() noexcept   { return juce::Colour::fromRGB (184, 188, 198); }
};

inline juce::Colour accentForFamily (const juce::String& family)
{
    if      (family.startsWithIgnoreCase ("GLS")) return juce::Colour::fromRGB (0, 209, 199);
    else if (family.startsWithIgnoreCase ("AEV")) return juce::Colour::fromRGB (75, 179, 255);
    else if (family.startsWithIgnoreCase ("MDL")) return juce::Colour::fromRGB (255, 95, 209);
    else if (family.startsWithIgnoreCase ("GRD")) return juce::Colour::fromRGB (255, 138, 60);
    else if (family.startsWithIgnoreCase ("PIT")) return juce::Colour::fromRGB (183, 117, 255);
    else if (family.startsWithIgnoreCase ("EQ"))  return juce::Colour::fromRGB (75, 224, 133);
    else if (family.startsWithIgnoreCase ("DYN")) return juce::Colour::fromRGB (255, 86, 86);
    else if (family.startsWithIgnoreCase ("UTL")) return juce::Colour::fromRGB (255, 216, 77);
    return juce::Colours::white;
}

inline juce::Image getGoodluckLogoImage()
{
    static juce::Image logo = juce::ImageFileFormat::loadFrom (logo_data::goodluck_logo_png,
                                                               logo_data::goodluck_logo_pngSize);
    return logo;
}

inline juce::Font makeFont (float size, bool bold = false)
{
    const auto flags = bold ? juce::Font::bold : juce::Font::plain;
    return juce::Font (juce::FontOptions (size, flags));
}

class GoodluckLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoodluckLookAndFeel()
    {
        setColour (juce::Slider::textBoxBackgroundColourId, Colours::panel());
        setColour (juce::Slider::textBoxTextColourId, Colours::text());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Colours::text());
        setColour (juce::ToggleButton::textColourId, Colours::text());
    }

    void setAccentColour (juce::Colour newAccent) { accent = newAccent; }
    juce::Colour getAccentColour() const noexcept { return accent; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> (x, y, (float) width, (float) height).reduced (4.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre = bounds.getCentre();

        g.setColour (Colours::panel());
        g.fillEllipse (bounds);

        g.setColour (Colours::outline());
        g.drawEllipse (bounds, 1.5f);

        auto angleRange = rotaryEndAngle - rotaryStartAngle;
        auto toAngle = rotaryStartAngle + sliderPosProportional * angleRange;

        auto arcRadius = radius - 5.0f;
        juce::Path filledArc;
        filledArc.addArc (centre.x - arcRadius, centre.y - arcRadius,
                          arcRadius * 2.0f, arcRadius * 2.0f,
                          rotaryStartAngle, toAngle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId, true).withMultipliedAlpha (0.9f));
        g.strokePath (filledArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path pointer;
        auto pointerLength = radius - 8.0f;
        auto pointerThickness = 3.0f;
        pointer.addLineSegment ({ centre.x, centre.y,
                                  centre.x + pointerLength * std::cos (toAngle),
                                  centre.y + pointerLength * std::sin (toAngle) }, pointerThickness);

        g.setColour (Colours::text());
        g.strokePath (pointer, juce::PathStrokeType (pointerThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        juce::ignoreUnused (minSliderPos, maxSliderPos);
        auto bounds = juce::Rectangle<float> (x, y, (float) width, (float) height);
        auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), 4.0f);

        g.setColour (Colours::outline());
        g.fillRoundedRectangle (track, 2.0f);

        auto thumbWidth = 12.0f;
        auto thumbHeight = 18.0f;
        juce::Rectangle<float> thumb (slider.isHorizontal() ? sliderPos - thumbWidth * 0.5f : bounds.getCentreX() - thumbWidth * 0.5f,
                                      slider.isHorizontal() ? bounds.getCentreY() - thumbHeight * 0.5f : sliderPos - thumbHeight * 0.5f,
                                      thumbWidth,
                                      thumbHeight);

        g.setColour (slider.findColour (juce::Slider::trackColourId, true).isTransparent()
                     ? accent : slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (thumb, 3.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto corner = 12.0f;

        auto active = button.getToggleState();
        auto fillColour = active ? accent : Colours::panel();
        auto outlineColour = active ? accent : Colours::outline();

        g.setColour (fillColour.withMultipliedAlpha (active ? 1.0f : 0.6f));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (outlineColour.withMultipliedAlpha (0.85f));
        g.drawRoundedRectangle (bounds, corner, 1.5f);

        g.setColour (Colours::text());
        g.setFont (makeFont (12.0f, true));
        auto text = button.getButtonText().isNotEmpty() ? button.getButtonText()
                                                        : (active ? "ON" : "OFF");
        g.drawFittedText (text, bounds.toNearestInt(), juce::Justification::centred, 1);
    }

private:
    juce::Colour accent { juce::Colours::white };
};

class GoodluckHeader : public juce::Component
{
public:
    GoodluckHeader (juce::String skuId, juce::String marketingName)
        : sku (std::move (skuId)), marketing (std::move (marketingName))
    {
        logo = getGoodluckLogoImage();
    }

    void setAccentColour (juce::Colour newColour)
    {
        accent = newColour;
        repaint();
    }

    void setPresetName (const juce::String& newPreset)
    {
        preset = newPreset;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::background());
        auto area = getLocalBounds();

        juce::Rectangle<int> logoBounds (16, 16, 32, 32);
        if (logo.isValid())
            g.drawImageWithin (logo, logoBounds.getX(), logoBounds.getY(),
                               logoBounds.getWidth(), logoBounds.getHeight(), juce::RectanglePlacement::centred);

        g.setColour (Colours::text());
        g.setFont (makeFont (20.0f, true));
        auto title = sku + " — " + marketing;
        g.drawFittedText (title, area.removeFromLeft (area.getWidth() / 2).withTrimmedTop (8), juce::Justification::centredLeft, 1);

        g.setFont (makeFont (13.0f));
        g.setColour (Colours::textSecondary());
        juce::String presetText = preset.isNotEmpty() ? preset : "Preset: Init";
        g.drawFittedText (presetText, area.removeFromTop (32), juce::Justification::centredRight, 1);

        g.setColour (accent.withMultipliedAlpha (0.8f));
        g.fillRect (getLocalBounds().removeFromBottom (2));
    }

private:
    juce::String sku;
    juce::String marketing;
    juce::String preset;
    juce::Colour accent { juce::Colours::white };
    juce::Image logo;
};

class GoodluckFooter : public juce::Component
{
public:
    void setAccentColour (juce::Colour newColour)
    {
        accent = newColour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::background());
        g.setColour (Colours::outline());
        g.drawRect (getLocalBounds());

        g.setColour (accent.withMultipliedAlpha (0.5f));
        g.fillRect (getLocalBounds().removeFromTop (2));
    }

private:
    juce::Colour accent { juce::Colours::white };
};
} // namespace gls::ui
