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
    : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

void PITGrowlWarpAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    dryBuffer.setSize (getTotalNumInputChannels(), samplesPerBlock);
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

    dryBuffer.makeCopyOf (buffer, true);

    const auto semitonesDown = apvts.getRawParameterValue (kParamSemitonesDown)->load();
    const auto growl         = apvts.getRawParameterValue (kParamGrowl)->load();
    const auto formant       = apvts.getRawParameterValue (kParamFormant)->load();
    const auto drive         = apvts.getRawParameterValue (kParamDrive)->load();
    const auto mix           = apvts.getRawParameterValue (kParamMix)->load();

    juce::ignoreUnused (semitonesDown, growl, formant, drive, currentSampleRate);

    // TODO: Implement full GrowlWarp path:
    // 1. Pitch shift input downward by `semitonesDown`.
    // 2. Apply formant filter to emphasize vocal-like contour.
    // 3. Inject growl/grit harmonics based on `growl` parameter.
    // 4. Apply additional drive/saturation stage.
    // 5. Blend processed signal with dry using `mix`.

    buffer.applyGain (mix);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, dryBuffer, ch, 0, numSamples, 1.0f - mix);
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
