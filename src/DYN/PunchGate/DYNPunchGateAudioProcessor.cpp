#include "DYNPunchGateAudioProcessor.h"

DYNPunchGateAudioProcessor::DYNPunchGateAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PUNCH_GATE", createParameterLayout())
{
}

void DYNPunchGateAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    for (auto& state : channelStates)
    {
        state.envelope = 0.0f;
        state.holdCounter = 0.0f;
        state.gateGain = 1.0f;
    }
}

void DYNPunchGateAudioProcessor::releaseResources()
{
}

void DYNPunchGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb   = read ("thresh");
    const auto rangeDb    = juce::jmax (0.0f, read ("range"));
    const auto attackMs   = juce::jmax (0.1f, read ("attack"));
    const auto holdMs     = juce::jmax (0.0f, read ("hold"));
    const auto releaseMs  = juce::jmax (1.0f, read ("release"));
    const auto hysteresis = juce::jmax (0.0f, read ("hysteresis"));
    const auto punchBoost = juce::Decibels::decibelsToGain (read ("punch_boost"));

    ensureStateSize();

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto openThresh   = juce::Decibels::decibelsToGain (threshDb);
    const auto closeThresh  = juce::Decibels::decibelsToGain (threshDb + hysteresis);
    const auto attenuation  = juce::Decibels::decibelsToGain (-rangeDb);
    const auto holdSamples  = holdMs * 0.001f * currentSampleRate;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            const float level  = std::abs (sample);

            if (level > state.envelope)
                state.envelope = attackCoeff * state.envelope + (1.0f - attackCoeff) * level;
            else
                state.envelope = releaseCoeff * state.envelope + (1.0f - releaseCoeff) * level;

            if (state.envelope >= openThresh)
            {
                state.holdCounter = holdSamples;
                state.gateGain = punchBoost;
            }
            else if (state.holdCounter > 0.0f)
            {
                state.holdCounter -= 1.0f;
            }
            else if (state.envelope <= closeThresh)
            {
                state.gateGain += 0.01f * (attenuation - state.gateGain);
            }

            data[i] = sample * state.gateGain;

            if (state.gateGain > 1.0f)
                state.gateGain += 0.003f * (1.0f - state.gateGain);
        }
    }
}

void DYNPunchGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNPunchGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNPunchGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",     "Threshold",
                                                                   juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("range",      "Range",
                                                                   juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",     "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.35f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hold",       "Hold",
                                                                   juce::NormalisableRange<float> (0.0f, 200.0f, 0.01f, 0.35f), 20.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",    "Release",
                                                                   juce::NormalisableRange<float> (5.0f, 500.0f, 0.01f, 0.3f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hysteresis", "Hysteresis",
                                                                   juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("punch_boost","Punch Boost",
                                                                   juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 4.0f));

    return { params.begin(), params.end() };
}

DYNPunchGateAudioProcessorEditor::DYNPunchGateAudioProcessorEditor (DYNPunchGateAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& s, const juce::String& label) { initialiseSlider (s, label); };

    makeSlider (threshSlider,     "Thresh");
    makeSlider (rangeSlider,      "Range");
    makeSlider (attackSlider,     "Attack");
    makeSlider (holdSlider,       "Hold");
    makeSlider (releaseSlider,    "Release");
    makeSlider (hysteresisSlider, "Hysteresis");
    makeSlider (punchBoostSlider, "Punch");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "thresh", "range", "attack", "hold", "release", "hysteresis", "punch_boost" };
    juce::Slider* sliders[]      = { &threshSlider, &rangeSlider, &attackSlider, &holdSlider,
                                     &releaseSlider, &hysteresisSlider, &punchBoostSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (700, 300);
}

void DYNPunchGateAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNPunchGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Punch Gate", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNPunchGateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto topRow = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (topRow, { &threshSlider, &rangeSlider, &attackSlider, &holdSlider });
    layoutRow (area,   { &releaseSlider, &hysteresisSlider, &punchBoostSlider });
}

juce::AudioProcessorEditor* DYNPunchGateAudioProcessor::createEditor()
{
    return new DYNPunchGateAudioProcessorEditor (*this);
}

void DYNPunchGateAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}
