#include "PITShimmerFallAudioProcessor.h"

namespace
{
constexpr auto kParamPitchInterval = "pitch_interval";
constexpr auto kParamFeedback      = "feedback";
constexpr auto kParamDamping       = "damping";
constexpr auto kParamTime          = "time";
constexpr auto kParamMix           = "mix";
} // namespace

//==============================================================================
PITShimmerFallAudioProcessor::PITShimmerFallAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

//==============================================================================
//==============================================================================
void PITShimmerFallAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSpec = { sampleRate, static_cast<juce::uint32> (samplesPerBlock), static_cast<juce::uint32> (getTotalNumInputChannels()) };
    reverb.prepare (currentSpec);
    shimmerBuffer.setSize (getTotalNumInputChannels(), samplesPerBlock);
    updateReverbParams();
}

void PITShimmerFallAudioProcessor::releaseResources()
{
    reverb.reset();
    shimmerBuffer.setSize (0, 0);
}

bool PITShimmerFallAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void PITShimmerFallAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples        = buffer.getNumSamples();

    for (auto ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Cache parameter values for this block.
    const auto pitchInterval = apvts.getRawParameterValue (kParamPitchInterval)->load();
    const auto feedback      = apvts.getRawParameterValue (kParamFeedback)->load();
    const auto damping       = apvts.getRawParameterValue (kParamDamping)->load();
    const auto timeSeconds   = apvts.getRawParameterValue (kParamTime)->load();
    const auto mix           = apvts.getRawParameterValue (kParamMix)->load();

    updateReverbParams();
    juce::ignoreUnused (pitchInterval, feedback, damping, timeSeconds);

    shimmerBuffer.makeCopyOf (buffer, true);

    // TODO: Implement shimmer flow:
    // 1. Run input through base reverb.
    // 2. Pitch shift the feedback path by `pitchInterval`.
    // 3. Apply damping/time controls to reverb feedback network.
    // 4. Mix processed signal with original using `mix`.

    // Placeholder: pass dry signal through until DSP is implemented.
    buffer.applyGain (mix);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, shimmerBuffer, ch, 0, numSamples, 1.0f - mix);
}

juce::AudioProcessorEditor* PITShimmerFallAudioProcessor::createEditor()
{
    return new PITShimmerFallAudioProcessorEditor (*this);
}

//==============================================================================
void PITShimmerFallAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PITShimmerFallAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PITShimmerFallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamPitchInterval, "Pitch Interval",
                                                                      juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 7.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamFeedback, "Feedback",
                                                                      juce::NormalisableRange<float> (0.0f, 0.98f, 0.001f), 0.6f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamDamping, "Damping",
                                                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamTime, "Time",
                                                                      juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f), 4.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamMix, "Mix",
                                                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

void PITShimmerFallAudioProcessor::updateReverbParams()
{
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.damping      = apvts.getRawParameterValue (kParamDamping)->load();
    reverbParams.roomSize     = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (kParamTime)->load() / 20.0f);
    reverbParams.wetLevel     = apvts.getRawParameterValue (kParamMix)->load();
    reverbParams.dryLevel     = 1.0f - reverbParams.wetLevel;
    reverbParams.freezeMode   = 0.0f;
    reverbParams.width        = 1.0f;
    reverb.setParameters (reverbParams);
}

//==============================================================================
PITShimmerFallAudioProcessorEditor::PITShimmerFallAudioProcessorEditor (PITShimmerFallAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    setSize (480, 300);

    initSlider (pitchIntervalSlider, "Pitch Interval");
    initSlider (feedbackSlider,      "Feedback");
    initSlider (dampingSlider,       "Damping");
    initSlider (timeSlider,          "Time");
    initSlider (mixSlider,           "Mix");

    auto& vts = processorRef.getValueTreeState();
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamPitchInterval, pitchIntervalSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamFeedback,      feedbackSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamDamping,       dampingSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamTime,          timeSlider));
    attachments.emplace_back (std::make_unique<SliderAttachment> (vts, kParamMix,           mixSlider));
}

void PITShimmerFallAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawText ("PIT Shimmer Fall", getLocalBounds().removeFromTop (30), juce::Justification::centred);
}

void PITShimmerFallAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto row  = area.removeFromTop (area.getHeight() / 2);

    pitchIntervalSlider.setBounds (row.removeFromLeft (area.getWidth() / 3));
    feedbackSlider.setBounds      (row.removeFromLeft (area.getWidth() / 3));
    dampingSlider.setBounds       (row);

    timeSlider.setBounds (area.removeFromLeft (area.getWidth() / 2));
    mixSlider.setBounds  (area);
}

void PITShimmerFallAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (slider);
    slider.setName (labelText);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PITShimmerFallAudioProcessor();
}
