#include "GLSChannelPilotAudioProcessor.h"

GLSChannelPilotAudioProcessor::GLSChannelPilotAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHANNEL_PILOT", createParameterLayout())
{
}

void GLSChannelPilotAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);

    const auto requiredChannels = getTotalNumOutputChannels();
    highPassFilters.resize (requiredChannels);
    lowPassFilters.resize (requiredChannels);

    for (size_t i = 0; i < highPassFilters.size(); ++i)
    {
        highPassFilters[i].reset();
        lowPassFilters[i].reset();
    }
}

void GLSChannelPilotAudioProcessor::releaseResources()
{
}

void GLSChannelPilotAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const auto inputTrimDb  = apvts.getRawParameterValue ("input_trim")->load();
    const auto hpfFreq      = apvts.getRawParameterValue ("hpf_freq")->load();
    const auto lpfFreq      = apvts.getRawParameterValue ("lpf_freq")->load();
    const auto phaseInvert  = apvts.getRawParameterValue ("phase")->load() > 0.5f;
    const auto panValue     = juce::jlimit (-1.0f, 1.0f, apvts.getRawParameterValue ("pan")->load());
    const auto outputTrimDb = apvts.getRawParameterValue ("output_trim")->load();

    if (static_cast<int> (highPassFilters.size()) != totalNumOutputChannels)
        prepareToPlay (currentSampleRate, 0);

    updateFilterCoefficients (hpfFreq, lpfFreq);

    const auto inputGain  = juce::Decibels::decibelsToGain (inputTrimDb);
    const auto outputGain = juce::Decibels::decibelsToGain (outputTrimDb);

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        auto& hpf = highPassFilters[juce::jlimit (0, (int) highPassFilters.size() - 1, ch)];
        auto& lpf = lowPassFilters[juce::jlimit (0, (int) lowPassFilters.size() - 1, ch)];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float value = channelData[sample] * inputGain;
            value = hpf.processSample (value);
            value = lpf.processSample (value);

            if (phaseInvert)
                value = -value;

            channelData[sample] = value;
        }
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1)
    {
        for (int ch = 1; ch < totalNumOutputChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
    }

    const float panAngle = (panValue + 1.0f) * (juce::MathConstants<float>::pi * 0.25f);
    const float panLeft  = std::cos (panAngle);
    const float panRight = std::sin (panAngle);

    if (totalNumOutputChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = left[i] * panLeft * outputGain;
            const float r = right[i] * panRight * outputGain;

            left[i]  = l;
            right[i] = r;
        }
    }
    else if (totalNumOutputChannels == 1)
    {
        auto* mono = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            mono[i] *= outputGain;
    }
}

void GLSChannelPilotAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSChannelPilotAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData (data, sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSChannelPilotAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dBRange = juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim",   "Input Trim", dBRange, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf_freq",     "HPF Freq",   juce::NormalisableRange<float> (20.0f, 1000.0f, 0.01f, 0.35f), 40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lpf_freq",     "LPF Freq",   juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.01f, 0.35f), 18000.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("phase",        "Phase", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pan",          "Pan", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",  "Output Trim", dBRange, 0.0f));

    return { params.begin(), params.end() };
}

GLSChannelPilotAudioProcessorEditor::GLSChannelPilotAudioProcessorEditor (GLSChannelPilotAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initialiseSlider (inputTrimSlider,  "Input");
    initialiseSlider (hpfSlider,        "HPF");
    initialiseSlider (lpfSlider,        "LPF");
    initialiseSlider (panSlider,        "Pan");
    initialiseSlider (outputTrimSlider, "Output");

    addAndMakeVisible (phaseButton);

    auto& state = processorRef.getValueTreeState();
    inputTrimAttachment   = std::make_unique<SliderAttachment> (state, "input_trim",  inputTrimSlider);
    hpfAttachment         = std::make_unique<SliderAttachment> (state, "hpf_freq",    hpfSlider);
    lpfAttachment         = std::make_unique<SliderAttachment> (state, "lpf_freq",    lpfSlider);
    phaseAttachment       = std::make_unique<ButtonAttachment> (state, "phase",       phaseButton);
    panAttachment         = std::make_unique<SliderAttachment> (state, "pan",         panSlider);
    outputTrimAttachment  = std::make_unique<SliderAttachment> (state, "output_trim", outputTrimSlider);

    setSize (520, 260);
}

void GLSChannelPilotAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSChannelPilotAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Channel Pilot", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSChannelPilotAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (200);

    auto column = top.getWidth() / 3;

    for (auto* slider : { &inputTrimSlider, &hpfSlider, &lpfSlider })
        slider->setBounds (top.removeFromLeft (column).reduced (10));

    auto bottom = area.removeFromTop (200);
    column = bottom.getWidth() / 3;

    phaseButton.setBounds (bottom.removeFromLeft (column).reduced (10));
    panSlider.setBounds (bottom.removeFromLeft (column).reduced (10));
    outputTrimSlider.setBounds (bottom.removeFromLeft (column).reduced (10));
}

juce::AudioProcessorEditor* GLSChannelPilotAudioProcessor::createEditor()
{
    return new GLSChannelPilotAudioProcessorEditor (*this);
}

void GLSChannelPilotAudioProcessor::updateFilterCoefficients (float hpfFreq, float lpfFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto hpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                        juce::jlimit<double> (10.0, currentSampleRate * 0.45, static_cast<double> (hpfFreq)));
    const auto lpf = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                       juce::jlimit<double> (50.0, currentSampleRate * 0.49, static_cast<double> (lpfFreq)));

    for (size_t i = 0; i < highPassFilters.size(); ++i)
    {
        highPassFilters[i].coefficients = hpf;
        lowPassFilters[i].coefficients  = lpf;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLSChannelPilotAudioProcessor();
}
