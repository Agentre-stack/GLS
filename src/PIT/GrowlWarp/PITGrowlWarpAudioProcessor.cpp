#include "PITGrowlWarpAudioProcessor.h"

namespace
{
constexpr auto kParamSemitonesDown = "semitones_down";
constexpr auto kParamGrowl         = "growl";
constexpr auto kParamFormant       = "formant";
constexpr auto kParamDrive         = "drive";
constexpr auto kParamMix           = "mix";
} // namespace

PITGrowlWarpAudioProcessor::PITGrowlWarpAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                                   .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

void PITGrowlWarpAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto totalChannels = juce::jmax (2, getTotalNumInputChannels());
    const auto blockSize = juce::jmax (1, samplesPerBlock);

    dryBuffer.setSize (totalChannels, blockSize);
    wetBuffer.setSize (totalChannels, blockSize);
    pitchShifter.prepare (currentSampleRate, totalChannels);
    pitchShifter.reset();
    formantFilters.clear();
}

void PITGrowlWarpAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
}

bool PITGrowlWarpAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void PITGrowlWarpAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples        = buffer.getNumSamples();

    for (auto ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto channelCount = juce::jmax (1, buffer.getNumChannels());
    const auto samples = juce::jmax (1, numSamples);
    dryBuffer.setSize (channelCount, samples, false, false, true);
    wetBuffer.setSize (channelCount, samples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);

    const auto semitonesDown = apvts.getRawParameterValue (kParamSemitonesDown)->load();
    const auto growl         = apvts.getRawParameterValue (kParamGrowl)->load();
    const auto formant       = apvts.getRawParameterValue (kParamFormant)->load();
    const auto drive         = apvts.getRawParameterValue (kParamDrive)->load();
    const auto mix           = apvts.getRawParameterValue (kParamMix)->load();

    const float ratio = std::pow (2.0f, semitonesDown / 12.0f);
    pitchShifter.process (wetBuffer, ratio);

    auto ensureFilters = [this, channels = buffer.getNumChannels()]()
    {
        if ((int) formantFilters.size() < channels)
        {
            formantFilters.resize ((size_t) channels);
            for (auto& filter : formantFilters)
                filter.reset();
        }
    };
    ensureFilters();

    const float formantFreq = juce::jmap (formant, -12.0f, 12.0f, 200.0f, 3200.0f);
    auto formantCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, formantFreq, 1.2f);
    for (auto& filter : formantFilters)
        filter.coefficients = formantCoeffs;

    for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
    {
        auto* data = wetBuffer.getWritePointer (ch);
        auto& formantFilter = formantFilters[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = formantFilter.processSample (data[i]);
            const float growlShape = juce::dsp::FastMathApproximations::tanh (sample * juce::jmap (growl, 0.0f, 1.0f, 1.0f, 4.0f));
            sample = juce::jmap (growl, 0.0f, 1.0f, sample, growlShape);
            const float driveGain = juce::jmap (drive, 0.0f, 1.0f, 1.0f, 6.0f);
            sample = juce::dsp::FastMathApproximations::tanh (sample * driveGain);
            data[i] = sample;
        }
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dry = dryBuffer.getReadPointer (ch);
        auto* wet = wetBuffer.getReadPointer (ch);
        auto* out = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            out[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }
}

juce::AudioProcessorEditor* PITGrowlWarpAudioProcessor::createEditor()
{
    return new PITGrowlWarpAudioProcessorEditor (*this);
}

void PITGrowlWarpAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PITGrowlWarpAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout PITGrowlWarpAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamSemitonesDown, "Semitones Down",
                                                                      juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f), -7.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamGrowl, "Growl",
                                                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamFormant, "Formant",
                                                                      juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamDrive, "Drive",
                                                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
PITGrowlWarpAudioProcessorEditor::PITGrowlWarpAudioProcessorEditor (PITGrowlWarpAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    setSize (480, 300);

    initSlider (semitonesDownSlider, "Semitones");
    initSlider (growlSlider,         "Growl");
    initSlider (formantSlider,       "Formant");
    initSlider (driveSlider,         "Drive");
    initSlider (mixSlider,           "Mix");

    auto& vts = processorRef.getValueTreeState();
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamSemitonesDown, semitonesDownSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamGrowl,         growlSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamFormant,       formantSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamDrive,         driveSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamMix,           mixSlider));
}

void PITGrowlWarpAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::orange);
    g.setFont (18.0f);
    g.drawText ("PIT Growl Warp", getLocalBounds().removeFromTop (30), juce::Justification::centred);
}

void PITGrowlWarpAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto upper = area.removeFromTop (area.getHeight() / 2);

    semitonesDownSlider.setBounds (upper.removeFromLeft (area.getWidth() / 3));
    growlSlider.setBounds         (upper.removeFromLeft (area.getWidth() / 3));
    formantSlider.setBounds       (upper);

    driveSlider.setBounds (area.removeFromLeft (area.getWidth() / 2));
    mixSlider.setBounds   (area);
}

void PITGrowlWarpAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    slider.setName (labelText);
    addAndMakeVisible (slider);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PITGrowlWarpAudioProcessor();
}
