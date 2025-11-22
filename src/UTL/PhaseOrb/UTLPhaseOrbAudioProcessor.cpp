#include "UTLPhaseOrbAudioProcessor.h"

namespace
{
constexpr auto kStateId       = "PHASE_ORB";
constexpr auto kParamWidth    = "width";
constexpr auto kParamPhase    = "phase_shift";
constexpr auto kParamRate     = "orb_rate";
constexpr auto kParamDepth    = "orb_depth";
constexpr auto kParamTilt     = "tilt";
constexpr auto kParamMix      = "mix";
constexpr auto kParamOutput   = "output_gain";
constexpr auto kParamInputTrim= "input_trim";
constexpr auto kParamOutputTrim= "output_trim";
constexpr auto kParamBypass   = "ui_bypass";

inline float degreesToRadians (float degrees)
{
    return degrees * juce::MathConstants<float>::pi / 180.0f;
}

class PhaseOrbVisual : public juce::Component, private juce::Timer
{
public:
    PhaseOrbVisual (UTLPhaseOrbAudioProcessor& processorRef, juce::Colour accentColour)
        : processor (processorRef), accent (accentColour)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (12.0f);
        g.setColour (gls::ui::Colours::panel());
        g.fillRoundedRectangle (bounds, 20.0f);
        g.setColour (gls::ui::Colours::outline());
        g.drawRoundedRectangle (bounds, 20.0f, 1.6f);

        auto orbArea = bounds.reduced (24.0f);
        const float orbValue = processor.getOrbitalPhase();
        const float radius = orbArea.getHeight() * 0.35f;
        auto centre = orbArea.getCentre();

        g.setColour (gls::ui::Colours::grid());
        g.drawEllipse (orbArea, 1.2f);

        g.setColour (accent.withMultipliedAlpha (0.8f));
        juce::Path orbit;
        orbit.addEllipse (orbArea);
        g.strokePath (orbit, juce::PathStrokeType (1.8f));

        auto position = centre + juce::Point<float> (std::sin (orbValue) * radius,
                                                     std::cos (orbValue) * radius * 0.6f);
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (position));

        g.setColour (gls::ui::Colours::text());
        g.setFont (gls::ui::makeFont (13.0f));
        g.drawFittedText ("Phase orbit", bounds.removeFromBottom (24).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    void timerCallback() override { repaint(); }

private:
    UTLPhaseOrbAudioProcessor& processor;
    juce::Colour accent;
};
} // namespace

UTLPhaseOrbAudioProcessor::UTLPhaseOrbAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void UTLPhaseOrbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    lfoPhase = 0.0;
}

void UTLPhaseOrbAudioProcessor::releaseResources()
{
}

void UTLPhaseOrbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    if (apvts.getRawParameterValue (kParamBypass)->load() > 0.5f)
        return;

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float width      = juce::jlimit (0.0f, 2.5f, get (kParamWidth));
    const float mix        = juce::jlimit (0.0f, 1.0f, get (kParamMix));
    const float tiltDb     = juce::jlimit (-12.0f, 12.0f, get (kParamTilt));
    const float basePhase  = degreesToRadians (juce::jlimit (-180.0f, 180.0f, get (kParamPhase)));
    const float orbRate    = juce::jlimit (0.05f, 5.0f, get (kParamRate));
    const float orbDepth   = juce::jlimit (0.0f, 1.0f, get (kParamDepth));
    const float outputGain = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 6.0f, get (kParamOutput)));
    const float inputTrim  = juce::Decibels::decibelsToGain (get (kParamInputTrim));
    const float outputTrim = juce::Decibels::decibelsToGain (get (kParamOutputTrim));

    buffer.applyGain (inputTrim);

    const double depthRadians = orbDepth * juce::MathConstants<double>::pi * 0.95;
    const double twoPi = juce::MathConstants<double>::twoPi;
    const double lfoIncrement = twoPi * (double) orbRate / currentSampleRate;

    const float sideGain = juce::Decibels::decibelsToGain (tiltDb * 0.5f) * width;
    const float midGain  = juce::Decibels::decibelsToGain (-tiltDb * 0.5f);
    const float dryGain  = 1.0f - mix;

    const bool hasStereo = numChannels > 1;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        lfoPhase += lfoIncrement;
        if (lfoPhase >= twoPi)
            lfoPhase -= twoPi;

        const double mod = std::sin (lfoPhase) * depthRadians;
        orbVisual.store ((float) std::fmod (lfoPhase, twoPi));

        const float phase = basePhase + (float) mod;
        const float cosPhase = std::cos (phase);
        const float sinPhase = std::sin (phase);

        const float leftIn  = buffer.getSample (0, sample);
        const float rightIn = hasStereo ? buffer.getSample (1, sample) : leftIn;

        float mid  = 0.5f * (leftIn + rightIn) * midGain;
        float side = 0.5f * (leftIn - rightIn) * sideGain;

        const float rotatedMid  = mid * cosPhase - side * sinPhase;
        const float rotatedSide = mid * sinPhase + side * cosPhase;

        const float wetLeft  = (rotatedMid + rotatedSide) * outputGain;
        const float wetRight = (rotatedMid - rotatedSide) * outputGain;

        buffer.setSample (0, sample, dryGain * leftIn + mix * wetLeft);

        if (hasStereo)
            buffer.setSample (1, sample, dryGain * rightIn + mix * wetRight);
    }

    buffer.applyGain (outputTrim);
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLPhaseOrbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamWidth, "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 2.5f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamPhase, "Phase Shift",
                                                                   juce::NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamRate, "Orb Rate",
                                                                   juce::NormalisableRange<float> (0.05f, 5.0f, 0.001f, 0.4f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamDepth, "Orb Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamTilt, "Tilt",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 6.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInputTrim, "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutputTrim, "Output Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

UTLPhaseOrbAudioProcessorEditor::UTLPhaseOrbAudioProcessorEditor (UTLPhaseOrbAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      processorRef (processor),
      accentColour (gls::ui::accentForFamily ("UTL")),
      headerComponent ("UTL.PhaseOrb", "Phase Orb")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    heroVisual = std::make_unique<PhaseOrbVisual> (processorRef, accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);
    addAndMakeVisible (heroVisual.get());

    configureRotarySlider (widthSlider, "Width");
    configureRotarySlider (phaseSlider, "Phase Shift");
    configureRotarySlider (rateSlider, "Orb Rate");
    configureRotarySlider (depthSlider, "Orb Depth");
    configureRotarySlider (tiltSlider, "Tilt");
    configureRotarySlider (mixSlider, "Mix");
    configureLinearSlider (inputTrimSlider, "Input Trim");
    configureLinearSlider (outputTrimSlider, "Output Trim");
    configureToggle (bypassButton, "Soft Bypass");

    auto& state = processorRef.getValueTreeState();
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamWidth,     widthSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamPhase,     phaseSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamRate,      rateSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamDepth,     depthSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamTilt,      tiltSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamMix,       mixSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamInputTrim, inputTrimSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, kParamOutputTrim,outputTrimSlider));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    addAndMakeVisible (widthSlider);
    addAndMakeVisible (phaseSlider);
    addAndMakeVisible (rateSlider);
    addAndMakeVisible (depthSlider);
    addAndMakeVisible (tiltSlider);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (inputTrimSlider);
    addAndMakeVisible (outputTrimSlider);
    addAndMakeVisible (bypassButton);

    setSize (880, 520);
}

UTLPhaseOrbAudioProcessorEditor::~UTLPhaseOrbAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void UTLPhaseOrbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
}

void UTLPhaseOrbAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerArea = bounds.removeFromTop (72);
    auto footerArea = bounds.removeFromBottom (72);
    headerComponent.setBounds (headerArea);
    footerComponent.setBounds (footerArea);

    auto body = bounds.reduced (16);
    auto macroArea = body.removeFromLeft ((int) (body.getWidth() * 0.32f)).reduced (8);
    auto heroArea  = body.removeFromLeft ((int) (body.getWidth() * 0.40f)).reduced (8);
    auto microArea = body.reduced (8);

    heroVisual->setBounds (heroArea);

    auto layoutColumn = [](juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps)
    {
        const int rowHeight = area.getHeight() / (int) comps.size();
        int y = area.getY();
        for (auto* comp : comps)
        {
            comp->setBounds (area.getX(), y, area.getWidth(), rowHeight);
            y += rowHeight;
        }
    };

    layoutColumn (macroArea,
    {
        &widthSlider, &phaseSlider, &rateSlider, &depthSlider, &tiltSlider
    });

    layoutColumn (microArea,
    {
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    });

    bypassButton.setBounds (microArea.removeFromBottom (42).reduced (12));
}

void UTLPhaseOrbAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (13.0f));
    label->attachToComponent (&slider, false);
    labels.push_back (std::move (label));
}

void UTLPhaseOrbAudioProcessorEditor::configureLinearSlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::trackColourId, accentColour);

    auto label = std::make_unique<juce::Label>();
    label->setText (labelText, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (gls::ui::makeFont (12.0f));
    label->attachToComponent (&slider, false);
    labels.push_back (std::move (label));
}

void UTLPhaseOrbAudioProcessorEditor::configureToggle (juce::ToggleButton& toggle, const juce::String& labelText)
{
    toggle.setButtonText (labelText);
    toggle.setColour (juce::ToggleButton::tickColourId, accentColour);
}

juce::AudioProcessorEditor* UTLPhaseOrbAudioProcessor::createEditor()
{
    return new UTLPhaseOrbAudioProcessorEditor (*this);
}

void UTLPhaseOrbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void UTLPhaseOrbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLPhaseOrbAudioProcessor();
}
