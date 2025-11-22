#include "GRDTransTubeXAudioProcessor.h"

namespace
{
constexpr auto paramDrive      = "drive";
constexpr auto paramTransSens  = "trans_sens";
constexpr auto paramAttackBias = "attack_bias";
constexpr auto paramTone       = "tone";
constexpr auto paramMix        = "mix";
constexpr auto paramInput      = "input_trim";
constexpr auto paramOutput     = "output_trim";
constexpr auto paramBypass     = "ui_bypass";
constexpr auto kStateId        = "TRANS_TUBE_X";
}

const std::array<GRDTransTubeXAudioProcessor::Preset, 3> GRDTransTubeXAudioProcessor::presetBank {{
    { "Punch Tube", {
        { paramDrive,       0.65f },
        { paramTransSens,   0.55f },
        { paramAttackBias,  0.35f },
        { paramTone,     7500.0f },
        { paramMix,         0.7f },
        { paramInput,       0.0f },
        { paramOutput,     -0.5f },
        { paramBypass,      0.0f }
    }},
    { "Sustain Glue", {
        { paramDrive,       0.55f },
        { paramTransSens,   0.3f },
        { paramAttackBias,  0.75f },
        { paramTone,     6200.0f },
        { paramMix,         0.65f },
        { paramInput,      -0.5f },
        { paramOutput,      0.0f },
        { paramBypass,      0.0f }
    }},
    { "Bright Crush", {
        { paramDrive,       0.75f },
        { paramTransSens,   0.6f },
        { paramAttackBias,  0.45f },
        { paramTone,     9000.0f },
        { paramMix,         0.8f },
        { paramInput,       0.0f },
        { paramOutput,     -1.0f },
        { paramBypass,      0.0f }
    }}
}};

GRDTransTubeXAudioProcessor::GRDTransTubeXAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDTransTubeXAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (44100.0, sampleRate);
    trackers.clear();
    toneFilters.clear();
    dryBuffer.setSize (getTotalNumOutputChannels(), 0);
}

void GRDTransTubeXAudioProcessor::releaseResources()
{
}

void GRDTransTubeXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    dryBuffer.makeCopyOf (buffer, true);

    const auto drive     = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramDrive)->load());
    const auto sens      = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramTransSens)->load());
    const auto attack    = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramAttackBias)->load());
    const auto toneHz    = juce::jlimit (500.0f, 12000.0f, apvts.getRawParameterValue (paramTone)->load());
    const auto mix       = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramMix)->load());
    const auto inputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (paramInput)->load());
    const auto outputGain= juce::Decibels::decibelsToGain (apvts.getRawParameterValue (paramOutput)->load());
    const bool bypassed  = apvts.getRawParameterValue (paramBypass)->load() > 0.5f;

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    const float driveGain = juce::jmap (drive, 1.0f, 18.0f);
    const float transientScale = juce::jmap (sens, 0.0f, 1.0f, 0.0f, 4.0f);
    const float attackBlend = juce::jmap (attack, 0.0f, 1.0f, 0.2f, 0.95f);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, toneHz, 0.707f);
    for (auto& filter : toneFilters)
        filter.coefficients = coeffs;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        auto* dryPtr   = dryBuffer.getReadPointer (ch);
        auto& tracker  = trackers[(size_t) ch];
        auto& toneFilter = toneFilters[(size_t) ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float drySample = dryPtr[sample];
            const float transient = tracker.process (drySample) * transientScale;

            const float driveMod = 1.0f + transient * attackBlend;
            const float attacked = drySample * (1.0f + transient * (1.0f - attackBlend));
            const float tubeIn   = attacked * driveGain * driveMod;

            float shaped = juce::dsp::FastMathApproximations::tanh (tubeIn);
            shaped = toneFilter.processSample (shaped);

            writePtr[sample] = (shaped * mix + drySample * (1.0f - mix)) * outputGain;
        }
    }
}

void GRDTransTubeXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDTransTubeXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDTransTubeXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramDrive, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.6f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTransSens, "Trans Sens",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramAttackBias, "Attack Bias",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTone, "Tone",
        juce::NormalisableRange<float> (500.0f, 12000.0f, 1.0f, 0.4f), 6000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMix, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramInput, "Input Trim",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramOutput, "Output Trim",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        paramBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GRDTransTubeXAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if ((int) trackers.size() < numChannels)
    {
        trackers.resize (numChannels);
        for (auto& tracker : trackers)
        {
            tracker.setSampleRate (currentSampleRate);
            tracker.setTimes (2.0f, 40.0f);
            tracker.reset();
        }
    }

    if ((int) toneFilters.size() < numChannels)
    {
        toneFilters.resize (numChannels);
        for (auto& filter : toneFilters)
            filter.reset();
    }

    if ((int) dryBuffer.getNumChannels() != numChannels
        || dryBuffer.getNumSamples() != numSamples)
        dryBuffer.setSize (numChannels, numSamples, false, false, true);
}

juce::AudioProcessorEditor* GRDTransTubeXAudioProcessor::createEditor()
{
    return new GRDTransTubeXAudioProcessorEditor (*this);
}

//==============================================================================
GRDTransTubeXAudioProcessorEditor::GRDTransTubeXAudioProcessorEditor (GRDTransTubeXAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.TransTubeX", "Trans Tube X")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    initSlider (driveSlider,      "Drive", true);
    initSlider (transSensSlider,  "Trans Sens", true);
    initSlider (attackBiasSlider, "Attack Bias");
    initSlider (toneSlider,       "Tone");
    initSlider (mixSlider,        "Mix");
    initSlider (inputTrimSlider,  "Input");
    initSlider (outputTrimSlider, "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDrive, driveSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTransSens, transSensSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramAttackBias, attackBiasSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTone, toneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMix, mixSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramInput, inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOutput, outputTrimSlider));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramBypass, bypassButton);

    setSize (760, 420);
}

void GRDTransTubeXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDTransTubeXAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 4;
    driveSlider     .setBounds (top.removeFromLeft (topWidth).reduced (8));
    transSensSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    attackBiasSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));
    toneSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 3;
    mixSlider        .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

void GRDTransTubeXAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void GRDTransTubeXAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDTransTubeXAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &driveSlider, &transSensSlider, &attackBiasSlider, &toneSlider,
        &mixSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDTransTubeXAudioProcessor();
}

int GRDTransTubeXAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDTransTubeXAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDTransTubeXAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDTransTubeXAudioProcessor::applyPreset (int index)
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
