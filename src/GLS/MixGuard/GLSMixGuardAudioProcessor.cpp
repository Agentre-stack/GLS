#include "GLSMixGuardAudioProcessor.h"

GLSMixGuardAudioProcessor::GLSMixGuardAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIX_GUARD", createParameterLayout())
{
}

void GLSMixGuardAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    limiterGain = 1.0f;
    loudnessAccumulator = 0.0f;
    loudnessSamples = 0;

    const auto blockSize = juce::jmax (1, samplesPerBlock);
    delaySpec.sampleRate = currentSampleRate;
    delaySpec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    delaySpec.numChannels = 1;
    delaySpecConfigured = true;

    maxDelaySamples = juce::jmax (1, juce::roundToInt (currentSampleRate * 0.05));
    delayCapacitySamples = juce::jmax (1, maxDelaySamples + 32);

    ensureStateSize();
    updateDelayCapacity();
}

void GLSMixGuardAudioProcessor::releaseResources()
{
}

void GLSMixGuardAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto ceilingDb  = get ("ceiling");
    const auto thresholdDb= get ("threshold");
    const auto lookahead  = get ("lookahead");
    const auto releaseMs  = get ("release");
    const auto targetLUFS = get ("target_lufs");
    const bool tpEnabled  = apvts.getRawParameterValue ("tp_enabled")->load() > 0.5f;

    juce::ignoreUnused (targetLUFS);

    ensureStateSize();
    const auto lookaheadSamples = juce::jlimit (0, maxDelaySamples, juce::roundToInt (lookahead * 0.001 * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto attackCoeff  = std::exp (-1.0f / (0.001f * currentSampleRate));
    const auto ceilingGain  = juce::Decibels::decibelsToGain (ceilingDb);
    const auto thresholdGain= juce::Decibels::decibelsToGain (thresholdDb);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (auto& state : channelStates)
        state.delayLine.setDelay (lookaheadSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            const float in = buffer.getSample (ch, sample);
            state.delayLine.pushSample (0, in);

            const float tp = measureTruePeak (state, in, tpEnabled);
            peak = juce::jmax (peak, tp);
        }

        const float targetGain = peak > thresholdGain ? thresholdGain / peak : 1.0f;
        if (targetGain < limiterGain)
            limiterGain = attackCoeff * (limiterGain - targetGain) + targetGain;
        else
            limiterGain = releaseCoeff * (limiterGain - targetGain) + targetGain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            float delayed = state.delayLine.popSample (0);
            delayed *= limiterGain * ceilingGain;
            buffer.setSample (ch, sample, delayed);

            loudnessAccumulator += delayed * delayed;
            ++loudnessSamples;
        }
    }
}

void GLSMixGuardAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSMixGuardAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSMixGuardAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ceiling",    "Ceiling",   juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("threshold",  "Threshold", juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f), -6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lookahead",  "Lookahead", juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.35f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",    "Release",   juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("target_lufs","Target LUFS",
                                                                   juce::NormalisableRange<float> (-30.0f, -6.0f, 0.1f), -14.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("tp_enabled", "TP Enabled", true));

    return { params.begin(), params.end() };
}

GLSMixGuardAudioProcessorEditor::GLSMixGuardAudioProcessorEditor (GLSMixGuardAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initialiseSlider (ceilingSlider,   "Ceiling");
    initialiseSlider (thresholdSlider, "Threshold");
    initialiseSlider (lookaheadSlider, "Lookahead");
    initialiseSlider (releaseSlider,   "Release");
    initialiseSlider (targetLufsSlider,"Target LUFS");
    addAndMakeVisible (tpButton);

    auto& state = processorRef.getValueTreeState();
    ceilingAttachment   = std::make_unique<SliderAttachment> (state, "ceiling",    ceilingSlider);
    thresholdAttachment = std::make_unique<SliderAttachment> (state, "threshold",  thresholdSlider);
    lookaheadAttachment = std::make_unique<SliderAttachment> (state, "lookahead",  lookaheadSlider);
    releaseAttachment   = std::make_unique<SliderAttachment> (state, "release",    releaseSlider);
    targetLufsAttachment= std::make_unique<SliderAttachment> (state, "target_lufs",targetLufsSlider);
    tpAttachment        = std::make_unique<ButtonAttachment> (state, "tp_enabled", tpButton);

    setSize (600, 260);
}

void GLSMixGuardAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSMixGuardAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Mix Guard", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSMixGuardAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto topRow = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (topRow, { &ceilingSlider, &thresholdSlider, &lookaheadSlider });
    layoutRow (area,   { &releaseSlider, &targetLufsSlider });
    tpButton.setBounds (area.removeFromRight (120).reduced (8));
}

juce::AudioProcessorEditor* GLSMixGuardAudioProcessor::createEditor()
{
    return new GLSMixGuardAudioProcessorEditor (*this);
}

void GLSMixGuardAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = juce::jmax (1, getTotalNumOutputChannels());
    const auto previousSize = channelStates.size();

    if (static_cast<int> (previousSize) == requiredChannels)
        return;

    channelStates.resize (static_cast<size_t> (requiredChannels));

    if (! delaySpecConfigured || delayCapacitySamples <= 0)
        return;

    for (size_t idx = previousSize; idx < channelStates.size(); ++idx)
        initialiseChannelState (channelStates[idx]);
}

void GLSMixGuardAudioProcessor::initialiseChannelState (ChannelState& state)
{
    if (! delaySpecConfigured || delayCapacitySamples <= 0)
        return;

    state.delayLine.prepare (delaySpec);
    state.delayLine.setMaximumDelayInSamples (delayCapacitySamples);
    state.delayLine.setDelay (0);
    state.delayLine.reset();
    state.previousSample = 0.0f;
}

float GLSMixGuardAudioProcessor::measureTruePeak (ChannelState& state, float currentSample, bool tpEnabled)
{
    if (! tpEnabled)
        return std::abs (currentSample);

    const float interpolated = 0.5f * (currentSample + state.previousSample);
    state.previousSample = currentSample;
    return juce::jmax (std::abs (currentSample), std::abs (interpolated));
}

void GLSMixGuardAudioProcessor::updateDelayCapacity()
{
    if (! delaySpecConfigured || delayCapacitySamples <= 0)
        return;

    for (auto& state : channelStates)
        initialiseChannelState (state);
}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSMixGuardAudioProcessor();
}
