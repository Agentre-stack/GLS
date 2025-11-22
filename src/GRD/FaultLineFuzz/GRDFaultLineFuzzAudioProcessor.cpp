#include "GRDFaultLineFuzzAudioProcessor.h"

namespace
{
constexpr auto paramInputTrim = "input_trim";
constexpr auto paramFuzz      = "fuzz";
constexpr auto paramBias      = "bias";
constexpr auto paramGate      = "gate";
constexpr auto paramTone      = "tone";
constexpr auto paramOutput    = "output_trim";
constexpr auto paramBypass    = "ui_bypass";
constexpr auto kStateId       = "FAULT_LINE_FUZZ";
}

const std::array<GRDFaultLineFuzzAudioProcessor::Preset, 3> GRDFaultLineFuzzAudioProcessor::presetBank {{
    { "Vocal Edge", {
        { paramInputTrim,   0.0f },
        { paramFuzz,        0.55f },
        { paramBias,        0.1f },
        { paramGate,        0.4f },
        { paramTone,     5200.0f },
        { paramOutput,     -1.0f },
        { paramBypass,      0.0f }
    }},
    { "Gritty Bass", {
        { paramInputTrim,   1.5f },
        { paramFuzz,        0.7f },
        { paramBias,       -0.15f },
        { paramGate,        0.2f },
        { paramTone,     3000.0f },
        { paramOutput,     -1.5f },
        { paramBypass,      0.0f }
    }},
    { "Alt Drum Crush", {
        { paramInputTrim,   0.5f },
        { paramFuzz,        0.65f },
        { paramBias,        0.0f },
        { paramGate,        0.5f },
        { paramTone,     6500.0f },
        { paramOutput,     -0.8f },
        { paramBypass,      0.0f }
    }}
}};

GRDFaultLineFuzzAudioProcessor::GRDFaultLineFuzzAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDFaultLineFuzzAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (44100.0, sampleRate);
    processingBuffer.setSize (getTotalNumOutputChannels(), 0);
    toneFilters.clear();
    gateState.assign ((size_t) getTotalNumOutputChannels(), 0.0f);
}

void GRDFaultLineFuzzAudioProcessor::releaseResources()
{
}

void GRDFaultLineFuzzAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    processingBuffer.makeCopyOf (buffer, true);

    const auto inputDb   = apvts.getRawParameterValue (paramInputTrim)->load();
    const auto fuzz      = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramFuzz)->load());
    const auto bias      = juce::jlimit (-1.0f, 1.0f, apvts.getRawParameterValue (paramBias)->load());
    const auto gate      = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramGate)->load());
    const auto toneHz    = apvts.getRawParameterValue (paramTone)->load();
    const auto outputDb  = apvts.getRawParameterValue (paramOutput)->load();
    const bool bypassed  = apvts.getRawParameterValue (paramBypass)->load() > 0.5f;

    const auto inGain  = juce::Decibels::decibelsToGain (inputDb);
    const auto outGain = juce::Decibels::decibelsToGain (outputDb);
    const float gateThreshold = juce::jmap (gate, 0.02f, 0.3f);
    const float gateRelease   = juce::jmap (gate, 0.1f, 0.6f);

    auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                        juce::jlimit (400.0f, 12000.0f, toneHz),
                                                                        0.707f);
    for (auto& filter : toneFilters)
        filter.coefficients = toneCoeffs;

    const float fuzzDrive = juce::jmap (fuzz, 2.0f, 40.0f);

    if (bypassed)
    {
        buffer.applyGain (outGain);
        return;
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        auto* procPtr  = processingBuffer.getWritePointer (ch);
        float& gateEnv = gateState[(size_t) ch];
        auto& toneFilter = toneFilters[(size_t) ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float x = procPtr[sample] * inGain;
            const float biased = x + bias * 0.5f;
            const float fuzzed = juce::dsp::FastMathApproximations::tanh (biased * fuzzDrive);

            const float level = std::abs (fuzzed);
            gateEnv = level > gateEnv ? level : gateEnv * gateRelease + level * (1.0f - gateRelease);
            const float gateGain = (gateEnv < gateThreshold) ? juce::jmap (gateEnv / gateThreshold, 0.0f, 1.0f, 0.0f, 1.0f)
                                                             : 1.0f;

            float toned = toneFilter.processSample (fuzzed * gateGain);
            writePtr[sample] = toned * outGain;
        }
    }
}

void GRDFaultLineFuzzAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDFaultLineFuzzAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDFaultLineFuzzAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramInputTrim, "Input Trim",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramFuzz, "Fuzz",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramBias, "Bias",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramGate, "Gate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTone, "Tone",
        juce::NormalisableRange<float> (400.0f, 12000.0f, 1.0f, 0.45f), 4500.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramOutput, "Output Trim",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        paramBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GRDFaultLineFuzzAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if ((int) processingBuffer.getNumChannels() != numChannels
        || processingBuffer.getNumSamples() != numSamples)
        processingBuffer.setSize (numChannels, numSamples, false, false, true);

    if ((int) gateState.size() < numChannels)
        gateState.resize ((size_t) numChannels, 0.0f);

    if ((int) toneFilters.size() < numChannels)
    {
        toneFilters.resize (numChannels);
        for (auto& filter : toneFilters)
            filter.reset();
    }
}

juce::AudioProcessorEditor* GRDFaultLineFuzzAudioProcessor::createEditor()
{
    return new GRDFaultLineFuzzAudioProcessorEditor (*this);
}

//==============================================================================
GRDFaultLineFuzzAudioProcessorEditor::GRDFaultLineFuzzAudioProcessorEditor (GRDFaultLineFuzzAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.FaultLineFuzz", "Fault Line Fuzz")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    initSlider (inputTrimSlider, "Input");
    initSlider (fuzzSlider,      "Fuzz", true);
    initSlider (biasSlider,      "Bias");
    initSlider (gateSlider,      "Gate");
    initSlider (toneSlider,      "Tone");
    initSlider (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramInputTrim, inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramFuzz, fuzzSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramBias, biasSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramGate, gateSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTone, toneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOutput, outputTrimSlider));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramBypass, bypassButton);

    setSize (740, 420);
}

void GRDFaultLineFuzzAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDFaultLineFuzzAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.6f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 4;
    inputTrimSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    fuzzSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));
    biasSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));
    gateSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 3;
    toneSlider      .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDFaultLineFuzzAudioProcessor();
}

void GRDFaultLineFuzzAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto lab = std::make_unique<juce::Label>();
    lab->setText (name, juce::dontSendNotification);
    lab->setJustificationType (juce::Justification::centred);
    lab->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    lab->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*lab);
    labels.push_back (std::move (lab));
}

void GRDFaultLineFuzzAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDFaultLineFuzzAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &inputTrimSlider, &fuzzSlider, &biasSlider, &gateSlider,
        &toneSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

int GRDFaultLineFuzzAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDFaultLineFuzzAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDFaultLineFuzzAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDFaultLineFuzzAudioProcessor::applyPreset (int index)
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
