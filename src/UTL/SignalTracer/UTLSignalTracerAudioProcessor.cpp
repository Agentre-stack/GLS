#include "UTLSignalTracerAudioProcessor.h"

namespace
{
constexpr auto kParamTapSelect   = "tap_select";
constexpr auto kParamPhaseView   = "phase_view";
constexpr auto kParamPeakHold    = "peak_hold";
constexpr auto kParamRmsWindow   = "rms_window";
constexpr auto kParamRoutingMode = "routing_mode";
} // namespace

UTLSignalTracerAudioProcessor::UTLSignalTracerAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

void UTLSignalTracerAudioProcessor::prepareToPlay (double, int) {}
void UTLSignalTracerAudioProcessor::releaseResources() {}

bool UTLSignalTracerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void UTLSignalTracerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples        = buffer.getNumSamples();

    for (auto ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto tapSelect   = apvts.getRawParameterValue (kParamTapSelect)->load();
    const auto phaseView   = apvts.getRawParameterValue (kParamPhaseView)->load();
    const auto peakHold    = apvts.getRawParameterValue (kParamPeakHold)->load();
    const auto rmsWindowMs = apvts.getRawParameterValue (kParamRmsWindow)->load();
    const auto routingMode = apvts.getRawParameterValue (kParamRoutingMode)->load();

    juce::ignoreUnused (tapSelect, phaseView, peakHold, rmsWindowMs, routingMode);

    // TODO: Implement analysis visualization taps, peak hold metering, RMS windowing,
    // and routing monitors. Audio output remains identical to input.
}

juce::AudioProcessorEditor* UTLSignalTracerAudioProcessor::createEditor()
{
    return new UTLSignalTracerAudioProcessorEditor (*this);
}

void UTLSignalTracerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void UTLSignalTracerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout UTLSignalTracerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.emplace_back (std::make_unique<juce::AudioParameterChoice> (kParamTapSelect, "Tap Select",
                                                                       juce::StringArray { "Input", "Pre", "Post", "Side" }, 0));
    params.emplace_back (std::make_unique<juce::AudioParameterChoice> (kParamPhaseView, "Phase View",
                                                                       juce::StringArray { "Lissajous", "Correlation", "Vectorscope" }, 0));
    params.emplace_back (std::make_unique<juce::AudioParameterBool> (kParamPeakHold, "Peak Hold", false));
    params.emplace_back (std::make_unique<juce::AudioParameterFloat> (kParamRmsWindow, "RMS Window (ms)",
                                                                      juce::NormalisableRange<float> (5.0f, 500.0f, 0.1f, 0.4f), 50.0f));
    params.emplace_back (std::make_unique<juce::AudioParameterChoice> (kParamRoutingMode, "Routing Mode",
                                                                       juce::StringArray { "Stereo", "Mid/Side", "Solo Tap" }, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
UTLSignalTracerAudioProcessorEditor::UTLSignalTracerAudioProcessorEditor (UTLSignalTracerAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    setSize (420, 260);

    tapBox.addItemList ({ "Input", "Pre", "Post", "Side" }, 1);
    phaseViewBox.addItemList ({ "Lissajous", "Correlation", "Vectorscope" }, 1);
    routingModeBox.addItemList ({ "Stereo", "Mid/Side", "Solo Tap" }, 1);

    rmsWindowSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    rmsWindowSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);

    addAndMakeVisible (tapBox);
    addAndMakeVisible (phaseViewBox);
    addAndMakeVisible (peakHoldButton);
    addAndMakeVisible (rmsWindowSlider);
    addAndMakeVisible (routingModeBox);

    auto& vts = processorRef.getValueTreeState();
    tapAttachment      = std::make_unique<ComboAttachment>  (vts, kParamTapSelect,   tapBox);
    phaseAttachment    = std::make_unique<ComboAttachment>  (vts, kParamPhaseView,   phaseViewBox);
    peakHoldAttachment = std::make_unique<ButtonAttachment> (vts, kParamPeakHold,    peakHoldButton);
    rmsAttachment      = std::make_unique<SliderAttachment> (vts, kParamRmsWindow,   rmsWindowSlider);
    routingAttachment  = std::make_unique<ComboAttachment>  (vts, kParamRoutingMode, routingModeBox);
}

void UTLSignalTracerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawText ("UTL Signal Tracer", getLocalBounds().removeFromTop (30), juce::Justification::centred);
}

void UTLSignalTracerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto row  = area.removeFromTop (40);
    tapBox.setBounds (row.removeFromLeft (area.getWidth() / 2));
    phaseViewBox.setBounds (row);

    row = area.removeFromTop (40);
    routingModeBox.setBounds (row.removeFromLeft (area.getWidth() / 2));
    peakHoldButton.setBounds (row);

    rmsWindowSlider.setBounds (area.reduced (80, 20));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLSignalTracerAudioProcessor();
}
