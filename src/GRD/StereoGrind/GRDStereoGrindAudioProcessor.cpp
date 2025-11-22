#include "GRDStereoGrindAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "STEREO_GRIND";
constexpr auto paramGrit    = "grit";
constexpr auto paramStereo  = "stereo";
constexpr auto paramDrive   = "drive";
constexpr auto paramMix     = "mix";
constexpr auto paramInput   = "input_trim";
constexpr auto paramOutput  = "output_trim";
constexpr auto paramBypass  = "ui_bypass";
}

const std::array<GRDStereoGrindAudioProcessor::Preset, 3> GRDStereoGrindAudioProcessor::presetBank {{
    { "Wide Grind", {
        { paramGrit,    0.5f },
        { paramStereo,  1.2f },
        { paramDrive,   0.45f },
        { paramMix,     0.7f },
        { paramInput,   0.0f },
        { paramOutput, -0.5f },
        { paramBypass,  0.0f }
    }},
    { "Mono Punch", {
        { paramGrit,    0.6f },
        { paramStereo,  0.4f },
        { paramDrive,   0.55f },
        { paramMix,     0.65f },
        { paramInput,   0.5f },
        { paramOutput, -1.0f },
        { paramBypass,  0.0f }
    }},
    { "Air Crush", {
        { paramGrit,    0.4f },
        { paramStereo,  1.0f },
        { paramDrive,   0.35f },
        { paramMix,     0.6f },
        { paramInput,   0.0f },
        { paramOutput, -0.8f },
        { paramBypass,  0.0f }
    }}
}};

GRDStereoGrindAudioProcessor::GRDStereoGrindAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDStereoGrindAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    dryBuffer.setSize (channels, (int) lastBlockSize);
}

void GRDStereoGrindAudioProcessor::releaseResources()
{
}

void GRDStereoGrindAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float grit    = juce::jlimit (0.0f, 1.0f, get (paramGrit));
    const float stereo  = juce::jlimit (0.0f, 1.5f, get (paramStereo));
    const float drive   = juce::jlimit (0.0f, 1.0f, get (paramDrive));
    const float mix     = juce::jlimit (0.0f, 1.0f, get (paramMix));
    const float inputGain  = juce::Decibels::decibelsToGain (juce::jlimit (-18.0f, 18.0f, get (paramInput)));
    const float outputGain = juce::Decibels::decibelsToGain (juce::jlimit (-18.0f, 18.0f, get (paramOutput)));
    const bool bypassed = get (paramBypass) > 0.5f;

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    buffer.applyGain (inputGain);
    if (bypassed)
    {
        buffer.applyGain (outputGain);
        return;
    }

    juce::AudioBuffer<float> midSide (2, numSamples);
    midSide.clear();

    if (numChannels >= 2)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float left  = dryBuffer.getSample (0, i);
            const float right = dryBuffer.getSample (1, i);
            midSide.setSample (0, i, 0.5f * (left + right));
            midSide.setSample (1, i, 0.5f * (left - right));
        }
    }
    else
    {
        midSide.copyFrom (0, 0, dryBuffer, 0, 0, numSamples);
        midSide.clear (1, 0, numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float mid  = midSide.getSample (0, i);
        float side = midSide.getSample (1, i);

        const float gritSample   = juce::dsp::FastMathApproximations::tanh (mid * (1.0f + drive * 4.0f));
        const float stereoSample = juce::dsp::FastMathApproximations::tanh (side * (1.0f + grit * 3.0f));

        midSide.setSample (0, i, juce::jmap (grit, mid, gritSample));
        midSide.setSample (1, i, juce::jmap (stereo, side, stereoSample));
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = midSide.getSample (0, i);
        const float side = midSide.getSample (1, i) * stereo;

        const float left  = mid + side;
        const float right = mid - side;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = dryBuffer.getSample (ch, i);
            const float wet = (ch == 0 ? left : right) * outputGain;
            buffer.setSample (ch, i, juce::jmap (mix, dry, wet));
        }
    }
}

void GRDStereoGrindAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDStereoGrindAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDStereoGrindAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramGrit, "Grit",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramStereo, "Stereo",
                                                                   juce::NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramDrive, "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramMix, "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramInput, "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (paramBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

int GRDStereoGrindAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDStereoGrindAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDStereoGrindAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDStereoGrindAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return;

    const auto& preset = presetBank[(size_t) index];
    for (const auto& entry : preset.params)
    {
        if (auto* param = apvts.getParameter (entry.first))
        {
            const auto norm = param->getNormalisableRange().convertTo0to1 (entry.second);
            param->setValueNotifyingHost (norm);
        }
    }
}

juce::AudioProcessorEditor* GRDStereoGrindAudioProcessor::createEditor()
{
    return new GRDStereoGrindAudioProcessorEditor (*this);
}

//==============================================================================
GRDStereoGrindAudioProcessorEditor::GRDStereoGrindAudioProcessorEditor (GRDStereoGrindAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.StereoGrind", "Stereo Grind")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false) { initSlider (slider, label, macro); };
    make (gritSlider,   "Grit", true);
    make (stereoSlider, "Stereo", true);
    make (driveSlider,  "Drive");
    make (mixSlider,    "Mix");
    make (inputTrimSlider, "Input");
    make (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramGrit, gritSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramStereo, stereoSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDrive, driveSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMix, mixSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramInput, inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOutput, outputTrimSlider));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramBypass, bypassButton);

    setSize (760, 420);
}

void GRDStereoGrindAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (label);
    addAndMakeVisible (slider);

    auto lab = std::make_unique<juce::Label>();
    lab->setText (label, juce::dontSendNotification);
    lab->setJustificationType (juce::Justification::centred);
    lab->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    lab->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*lab);
    labels.push_back (std::move (lab));
}

void GRDStereoGrindAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDStereoGrindAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &gritSlider, &stereoSlider, &driveSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

void GRDStereoGrindAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDStereoGrindAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 3;
    gritSlider   .setBounds (top.removeFromLeft (topWidth).reduced (8));
    stereoSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    driveSlider  .setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 3;
    mixSlider        .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDStereoGrindAudioProcessor();
}
