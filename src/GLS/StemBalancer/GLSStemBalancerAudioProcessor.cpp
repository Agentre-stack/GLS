#include "GLSStemBalancerAudioProcessor.h"

GLSStemBalancerAudioProcessor::GLSStemBalancerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STEM_BALANCER", createParameterLayout())
{
}

void GLSStemBalancerAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
}

void GLSStemBalancerAudioProcessor::releaseResources()
{
}

void GLSStemBalancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto stemGainDb = get ("stem_gain");
    const auto tilt       = get ("tilt");
    const auto presence   = get ("presence");
    const auto lowTight   = get ("low_tight");
    const bool autoGain   = apvts.getRawParameterValue ("auto_gain")->load() > 0.5f;

    ensureStateSize();
    updateFilters (tilt, presence, lowTight);

    const auto stemGain = juce::Decibels::decibelsToGain (stemGainDb);
    double preEnergy = 0.0;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);
        auto* lShelf = &state.lowShelf;
        auto* hShelf = &state.highShelf;
        auto* presenceFilter = &state.presenceBell;
        auto* lowTightFilter = &state.lowTightHpf;

        for (int i = 0; i < numSamples; ++i)
        {
            const float original = data[i];
            preEnergy += original * original;

            float sample = original;
            sample = lShelf->processSample (sample);
            sample = hShelf->processSample (sample);
            sample = presenceFilter->processSample (sample);
            sample = lowTightFilter->processSample (sample);
            sample *= stemGain;

            data[i] = sample;
        }
    }

    double postEnergy = 0.0;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            postEnergy += data[i] * data[i];
    }

    if (autoGain && postEnergy > 0.0 && preEnergy > 0.0)
    {
        const auto compensation = std::sqrt (preEnergy / postEnergy);
        buffer.applyGain ((float) compensation);
    }
}

void GLSStemBalancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSStemBalancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSStemBalancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("stem_gain", "Stem Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt",      "Tilt",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence",  "Presence",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_tight", "Low Tight",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("auto_gain", "Auto Gain", true));

    return { params.begin(), params.end() };
}

GLSStemBalancerAudioProcessorEditor::GLSStemBalancerAudioProcessorEditor (GLSStemBalancerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (stemGainSlider, "Stem Gain");
    initSlider (tiltSlider,     "Tilt");
    initSlider (presenceSlider, "Presence");
    initSlider (lowTightSlider, "Low Tight");
    addAndMakeVisible (autoGainButton);

    auto& state = processorRef.getValueTreeState();
    stemGainAttachment = std::make_unique<SliderAttachment> (state, "stem_gain", stemGainSlider);
    tiltAttachment     = std::make_unique<SliderAttachment> (state, "tilt",      tiltSlider);
    presenceAttachment = std::make_unique<SliderAttachment> (state, "presence",  presenceSlider);
    lowTightAttachment = std::make_unique<SliderAttachment> (state, "low_tight", lowTightSlider);
    autoGainAttachment = std::make_unique<ButtonAttachment> (state, "auto_gain", autoGainButton);

    setSize (520, 260);
}

void GLSStemBalancerAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GLSStemBalancerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Stem Balancer", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSStemBalancerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto width = area.getWidth() / 4;

    stemGainSlider.setBounds (area.removeFromLeft (width).reduced (8));
    tiltSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    presenceSlider.setBounds (area.removeFromLeft (width).reduced (8));
    lowTightSlider.setBounds (area.removeFromLeft (width).reduced (8));

    autoGainButton.setBounds (area.removeFromBottom (30).removeFromLeft (120));
}

juce::AudioProcessorEditor* GLSStemBalancerAudioProcessor::createEditor()
{
    return new GLSStemBalancerAudioProcessorEditor (*this);
}

void GLSStemBalancerAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
    {
        channelStates.resize (requiredChannels);
        juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) 512, 1 };
        for (auto& state : channelStates)
        {
            state.lowShelf.prepare (spec);
            state.highShelf.prepare (spec);
            state.presenceBell.prepare (spec);
            state.lowTightHpf.prepare (spec);
        }
    }
}

void GLSStemBalancerAudioProcessor::updateFilters (float tilt, float presenceDb, float lowTightAmount)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lowGain  = juce::Decibels::decibelsToGain (tilt);
    const auto highGain = juce::Decibels::decibelsToGain (-tilt);
    const auto presenceGain = juce::Decibels::decibelsToGain (presenceDb);
    const auto hpfFreq = juce::jmap (lowTightAmount, 20.0f, 160.0f);

    for (auto& state : channelStates)
    {
        state.lowShelf.coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 250.0f, 0.707f, lowGain);
        state.highShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 4000.0f, 0.707f, highGain);
        state.presenceBell.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 2500.0f, 0.9f, presenceGain);
        state.lowTightHpf.coefficients  = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpfFreq);
    }
}
