#include "DYNRMSRiderAudioProcessor.h"

DYNRMSRiderAudioProcessor::DYNRMSRiderAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RMS_RIDER", createParameterLayout())
{
}

void DYNRMSRiderAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    for (auto& state : channelStates)
    {
        state.lookaheadLine.reset();
        state.envelope = 0.0f;
    }
    gainSmoothed = 1.0f;
}

void DYNRMSRiderAudioProcessor::releaseResources()
{
}

void DYNRMSRiderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto targetDb   = read ("target_level");
    const auto speed      = juce::jlimit (0.0f, 1.0f, read ("speed"));
    const auto rangeDb    = juce::jlimit (0.0f, 24.0f, read ("range"));
    const auto hfSense    = juce::jlimit (0.0f, 1.0f, read ("hf_sensitivity"));
    const auto lookahead  = read ("lookahead");
    const auto outputTrim = juce::Decibels::decibelsToGain (read ("output_trim"));

    ensureStateSize();
    const auto lookaheadSamples = juce::roundToInt (lookahead * 0.001f * currentSampleRate);

    const auto attackCoeff  = std::exp (-1.0f / ((10.0f - speed * 9.5f) * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / ((50.0f + speed * 450.0f) * 0.001f * currentSampleRate));
    const auto targetGain   = juce::Decibels::decibelsToGain (targetDb);

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (auto& state : channelStates)
        state.lookaheadLine.setDelay (lookaheadSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float rms = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            const float in = buffer.getSample (ch, sample);

            float processed = in;
            if (hfSense > 0.0f)
            {
                static constexpr float alpha = 0.995f;
                processed = alpha * state.envelope + (1.0f - alpha) * (in - state.envelope);
            }

            state.envelope = attackCoeff * state.envelope + (1.0f - attackCoeff) * std::abs (processed);
            rms += state.envelope * state.envelope;

            state.lookaheadLine.pushSample (0, in);
        }
        rms = std::sqrt (rms / juce::jmax (1, numChannels));

        float gain = 1.0f;
        if (rms > 0.0f)
        {
            const float desired = targetGain / rms;
            const float rangeGain = juce::Decibels::decibelsToGain (rangeDb);
            gain = juce::jlimit (1.0f / rangeGain, rangeGain, desired);
        }

        if (gain < gainSmoothed)
            gainSmoothed = attackCoeff * (gainSmoothed - gain) + gain;
        else
            gainSmoothed = releaseCoeff * (gainSmoothed - gain) + gain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            float delayed = state.lookaheadLine.popSample (0);
            buffer.setSample (ch, sample, delayed * gainSmoothed * outputTrim);
        }
    }
}

void DYNRMSRiderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNRMSRiderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNRMSRiderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("target_level", "Target Level",
                                                                   juce::NormalisableRange<float> (-30.0f, -3.0f, 0.1f), -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("speed",        "Speed",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("range",        "Range",
                                                                   juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hf_sensitivity","HF Sensitivity",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lookahead",    "Lookahead",
                                                                   juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",  "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

DYNRMSRiderAudioProcessorEditor::DYNRMSRiderAudioProcessorEditor (DYNRMSRiderAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (targetLevelSlider,     "Target");
    make (speedSlider,           "Speed");
    make (rangeSlider,           "Range");
    make (hfSensitivitySlider,   "HF Sens" );
    make (lookaheadSlider,       "Lookahead");
    make (outputTrimSlider,      "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "target_level", "speed", "range", "hf_sensitivity", "lookahead", "output_trim" };
    juce::Slider* sliders[]      = { &targetLevelSlider, &speedSlider, &rangeSlider, &hfSensitivitySlider, &lookaheadSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 300);
}

void DYNRMSRiderAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNRMSRiderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN RMS Rider", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNRMSRiderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto rowHeight = area.getHeight() / 2;

    auto topRow = area.removeFromTop (rowHeight);
    targetLevelSlider    .setBounds (topRow.removeFromLeft (topRow.getWidth() / 3).reduced (8));
    speedSlider          .setBounds (topRow.removeFromLeft (topRow.getWidth() / 2).reduced (8));
    rangeSlider          .setBounds (topRow.reduced (8));

    auto bottomRow = area.removeFromTop (rowHeight);
    hfSensitivitySlider .setBounds (bottomRow.removeFromLeft (bottomRow.getWidth() / 3).reduced (8));
    lookaheadSlider     .setBounds (bottomRow.removeFromLeft (bottomRow.getWidth() / 2).reduced (8));
    outputTrimSlider    .setBounds (bottomRow.reduced (8));
}

juce::AudioProcessorEditor* DYNRMSRiderAudioProcessor::createEditor()
{
    return new DYNRMSRiderAudioProcessorEditor (*this);
}

void DYNRMSRiderAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}
