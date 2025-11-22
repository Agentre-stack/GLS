#include "GRDBassMaulAudioProcessor.h"

GRDBassMaulAudioProcessor::GRDBassMaulAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BASS_MAUL", createParameterLayout())
{
}

void GRDBassMaulAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureChannelState (juce::jmax (1, getTotalNumOutputChannels()));
}

void GRDBassMaulAudioProcessor::releaseResources()
{
}

void GRDBassMaulAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    auto get = [this](const char* id)
    {
        if (auto* param = apvts.getRawParameterValue (id))
            return param->load();
        return 0.0f;
    };

    const bool bypassed    = get ("ui_bypass") > 0.5f;
    if (bypassed)
        return;

    const float drive       = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float subBoostDb  = juce::jlimit (0.0f, 12.0f, get ("sub_boost"));
    const float tightnessHz = juce::jlimit (20.0f, 220.0f, get ("tightness"));
    const float blend       = juce::jlimit (0.0f, 1.0f, get ("blend"));
    const float trimDb      = juce::jlimit (-12.0f, 12.0f, get ("output_trim"));
    const float inputTrimDb = juce::jlimit (-24.0f, 24.0f, get ("input_trim"));

    const float driveGain = 1.0f + drive * 7.0f;
    const float subGain   = juce::Decibels::decibelsToGain (subBoostDb);
    const float trimGain  = juce::Decibels::decibelsToGain (trimDb);
    const float inputGain = juce::Decibels::decibelsToGain (inputTrimDb);

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureChannelState (numChannels);
    updateFilterCoefficients (tightnessHz, 120.0f);

    buffer.applyGain (inputGain);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelStates[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = data[i];
            float tight = state.tightHighpass.processSample (input);
            float shaped = std::tanh (tight * driveGain);
            const float subComponent = state.subLowpass.processSample (input) * subGain;
            const float processed = shaped + subComponent;
            data[i] = juce::jmap (blend, input, processed) * trimGain;
        }
    }
}

void GRDBassMaulAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDBassMaulAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDBassMaulAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_boost", "Sub Boost",
                                                                   juce::NormalisableRange<float> (0.0f, 12.0f, 0.01f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tightness", "Tightness",
                                                                   juce::NormalisableRange<float> (20.0f, 220.0f, 0.01f, 0.3f), 90.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("blend", "Blend",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim", "Input Trim",
                                                                   juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GRDBassMaulAudioProcessor::ensureChannelState (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) channelStates.size() < numChannels)
        channelStates.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (filterSpecSampleRate, currentSampleRate)
                             || filterSpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& state : channelStates)
        {
            state.tightHighpass.prepare (spec);
            state.tightHighpass.reset();
            state.subLowpass.prepare (spec);
            state.subLowpass.reset();
        }
        filterSpecSampleRate = currentSampleRate;
        filterSpecBlockSize  = targetBlock;
    }
}

void GRDBassMaulAudioProcessor::updateFilterCoefficients (float tightnessHz, float subSplitHz)
{
    if (currentSampleRate <= 0.0)
        return;

    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                 juce::jlimit (20.0f, 400.0f, tightnessHz), 0.9f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                juce::jlimit (40.0f, 250.0f, subSplitHz), 0.8f);

    for (auto& state : channelStates)
    {
        state.tightHighpass.coefficients = hp;
        state.subLowpass.coefficients = lp;
    }
}

class BassMaulVisual : public juce::Component, private juce::Timer
{
public:
    BassMaulVisual (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
        : apvts (state), accent (accentColour)
    {
        drive     = state.getRawParameterValue ("drive");
        subBoost  = state.getRawParameterValue ("sub_boost");
        tightness = state.getRawParameterValue ("tightness");
        blend     = state.getRawParameterValue ("blend");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (gls::ui::Colours::panel());
        auto area = getLocalBounds().toFloat().reduced (12.0f);

        g.setColour (gls::ui::Colours::grid());
        g.drawRoundedRectangle (area, 8.0f, 1.5f);

        auto driveAmount = drive != nullptr ? drive->load() : 0.5f;
        juce::Path transferCurve;
        const int steps = 80;
        for (int i = 0; i <= steps; ++i)
        {
            auto t = (float) i / (float) steps;
            auto x = area.getX() + t * area.getWidth();
            auto normalized = juce::jmap (t, 0.0f, 1.0f, -1.0f, 1.0f);
            auto sat = std::tanh (normalized * juce::jmap (driveAmount, 1.0f, 6.0f));
            auto y = juce::jmap (sat, -1.0f, 1.0f, area.getBottom() - area.getHeight() * 0.45f, area.getY());
            if (i == 0)
                transferCurve.startNewSubPath (x, y);
            else
                transferCurve.lineTo (x, y);
        }
        g.setColour (accent);
        g.strokePath (transferCurve, juce::PathStrokeType (2.0f));

        auto tightVal = tightness != nullptr ? tightness->load() : 90.0f;
        auto tightNorm = juce::jlimit (0.0f, 1.0f, (tightVal - 20.0f) / 200.0f);
        auto freqX = area.getX() + tightNorm * area.getWidth();
        g.setColour (accent.withMultipliedAlpha (0.4f));
        g.drawLine (freqX, area.getBottom(), freqX, area.getY(), 1.5f);

        auto subVal = subBoost != nullptr ? subBoost->load() : 0.0f;
        auto subHeight = juce::jmap (subVal, 0.0f, 12.0f, 0.0f, area.getHeight() * 0.35f);
        auto subArea = juce::Rectangle<float> (area.getX(), area.getBottom() - subHeight, area.getWidth(), subHeight);
        g.setColour (accent.withMultipliedAlpha (0.35f));
        g.fillRect (subArea);

        auto blendVal = blend != nullptr ? blend->load() : 0.5f;
        g.setColour (gls::ui::Colours::textSecondary());
        g.setFont (gls::ui::makeFont (12.0f));
        g.drawFittedText ("Dry/Wet " + juce::String (juce::roundToInt (blendVal * 100.0f)) + " %",
                          area.toNearestInt().removeFromTop (18), juce::Justification::centred, 1);
    }

    void timerCallback() override { repaint(); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;
    std::atomic<float>* drive = nullptr;
    std::atomic<float>* subBoost = nullptr;
    std::atomic<float>* tightness = nullptr;
    std::atomic<float>* blend = nullptr;
};

GRDBassMaulAudioProcessorEditor::GRDBassMaulAudioProcessorEditor (GRDBassMaulAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.BassMaul", "Bass Maul"),
      footerComponent()
{
    lookAndFeel.setAccentColour (accentColour);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    centerVisual = std::make_unique<BassMaulVisual> (processorRef.getValueTreeState(), accentColour);
    addAndMakeVisible (*centerVisual);

    configureSlider (driveSlider,     "Drive",       true);
    configureSlider (tightnessSlider, "Tightness",   true);
    configureSlider (subBoostSlider,  "Sub Boost",   false);
    configureSlider (trimSlider,      "Output Trim", false);
    configureSlider (blendSlider,     "Dry / Wet",   false, true);
    configureSlider (inputTrimSlider, "Input",       false, true);

    bypassButton.setButtonText ("Soft Bypass");
    bypassButton.setLookAndFeel (&lookAndFeel);
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);

    auto& state = processorRef.getValueTreeState();
    auto attachSlider = [this, &state](const char* paramID, juce::Slider& slider)
    {
        attachments.push_back (std::make_unique<SliderAttachment> (state, paramID, slider));
    };

    attachSlider ("drive",      driveSlider);
    attachSlider ("tightness",  tightnessSlider);
    attachSlider ("sub_boost",  subBoostSlider);
    attachSlider ("output_trim", trimSlider);
    attachSlider ("blend",      blendSlider);
    attachSlider ("input_trim", inputTrimSlider);

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (820, 520);
}

GRDBassMaulAudioProcessorEditor::~GRDBassMaulAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GRDBassMaulAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& label,
                                                       bool isMacro, bool isLinear)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (isLinear ? juce::Slider::LinearHorizontal
                                    : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 70 : 60, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour (juce::Slider::thumbColourId, accentColour);
    slider.setColour (juce::Slider::trackColourId, accentColour);
    addAndMakeVisible (slider);

    auto labelComponent = std::make_unique<juce::Label>();
    labelComponent->setText (label, juce::dontSendNotification);
    labelComponent->setJustificationType (juce::Justification::centred);
    labelComponent->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    labelComponent->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*labelComponent);

    labeledSliders.push_back ({ &slider, labelComponent.get() });
    sliderLabels.push_back (std::move (labelComponent));
}

void GRDBassMaulAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());

    auto body = getLocalBounds();
    body.removeFromTop (64);
    body.removeFromBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.3f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 8.0f);
}

void GRDBassMaulAudioProcessorEditor::layoutLabels()
{
    for (auto& entry : labeledSliders)
    {
        if (entry.slider == nullptr || entry.label == nullptr)
            continue;

        auto sliderBounds = entry.slider->getBounds();
        auto labelBounds = sliderBounds.withHeight (18).translated (0, -22);
        entry.label->setBounds (labelBounds);
    }
}

void GRDBassMaulAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto headerBounds = bounds.removeFromTop (64);
    auto footerBounds = bounds.removeFromBottom (64);
    headerComponent.setBounds (headerBounds);
    footerComponent.setBounds (footerBounds);

    auto body = bounds;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.33f)).reduced (12);
    auto right = body.removeFromRight (juce::roundToInt (body.getWidth() * 0.25f)).reduced (12);
    auto centre = body.reduced (12);

    if (centerVisual != nullptr)
        centerVisual->setBounds (centre);

    auto macroHeight = left.getHeight() / 2;
    driveSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));
    tightnessSlider.setBounds (left.removeFromTop (macroHeight).reduced (8));

    auto microHeight = right.getHeight() / 2;
    subBoostSlider.setBounds (right.removeFromTop (microHeight).reduced (8));
    trimSlider.setBounds (right.removeFromTop (microHeight).reduced (8));

    auto footerArea = footerBounds.reduced (32, 8);
    auto slotWidth = footerArea.getWidth() / 3;

    auto slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    inputTrimSlider.setBounds (slot);

    slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    blendSlider.setBounds (slot);

    slot = footerArea.removeFromLeft (slotWidth).reduced (8);
    bypassButton.setBounds (slot);

    layoutLabels();
}

juce::AudioProcessorEditor* GRDBassMaulAudioProcessor::createEditor()
{
    return new GRDBassMaulAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDBassMaulAudioProcessor();
}
