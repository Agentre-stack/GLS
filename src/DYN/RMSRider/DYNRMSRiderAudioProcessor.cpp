#include "DYNRMSRiderAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "RMS_RIDER";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<DYNRMSRiderAudioProcessor::Preset, 3> DYNRMSRiderAudioProcessor::presetBank {{
    { "Vocal Smooth", {
        { "target_level",   -18.0f },
        { "speed",            0.55f },
        { "range",            6.0f },
        { "hf_sensitivity",   0.4f },
        { "lookahead",        6.0f },
        { kParamInput,        0.0f },
        { kParamOutput,       0.5f },
        { kParamBypass,       0.0f }
    }},
    { "Mix Leveler", {
        { "target_level",   -20.0f },
        { "speed",            0.4f },
        { "range",            8.0f },
        { "hf_sensitivity",   0.5f },
        { "lookahead",        8.0f },
        { kParamInput,        0.0f },
        { kParamOutput,       0.0f },
        { kParamBypass,       0.0f }
    }},
    { "Broadcast Tight", {
        { "target_level",   -16.0f },
        { "speed",            0.7f },
        { "range",            10.0f },
        { "hf_sensitivity",   0.6f },
        { "lookahead",        5.0f },
        { kParamInput,       -1.0f },
        { kParamOutput,       0.0f },
        { kParamBypass,       0.0f }
    }}
}};

DYNRMSRiderAudioProcessor::DYNRMSRiderAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNRMSRiderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };

    for (auto& state : channelStates)
    {
        state.lookaheadLine.prepare (spec);
        state.lookaheadLine.reset();
        state.envelope = 0.0f;
    }

    gainSmoothed = 1.0f;
}

void DYNRMSRiderAudioProcessor::releaseResources()
{
}

void DYNRMSRiderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto targetDb   = read ("target_level");
    const auto speed      = juce::jlimit (0.0f, 1.0f, read ("speed"));
    const auto rangeDb    = juce::jlimit (0.0f, 24.0f, read ("range"));
    const auto hfSense    = juce::jlimit (0.0f, 1.0f, read ("hf_sensitivity"));
    const auto lookahead  = read ("lookahead");
    const auto inputTrim  = juce::Decibels::decibelsToGain (read (kParamInput));
    const auto outputTrim = juce::Decibels::decibelsToGain (read (kParamOutput));
    const bool bypassed   = read (kParamBypass) > 0.5f;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    ensureStateSize (numChannels);
    if (numSamples == 0 || numChannels == 0)
        return;

    buffer.applyGain (inputTrim);

    if (bypassed)
        return;

    const auto lookaheadSamples = juce::roundToInt (lookahead * 0.001f * currentSampleRate);

    const auto attackCoeff  = std::exp (-1.0f / ((10.0f - speed * 9.5f) * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / ((50.0f + speed * 450.0f) * 0.001f * currentSampleRate));
    const auto targetGain   = juce::Decibels::decibelsToGain (targetDb);

    for (auto& state : channelStates)
        state.lookaheadLine.setDelay (lookaheadSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float rms = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            const float in = buffer.getSample (ch, sample);

            float processed = in;
            if (hfSense > 0.0f)
            {
                static constexpr float alpha = 0.995f;
                processed = alpha * state.envelope + (1.0f - alpha) * (in - state.envelope);
            }

            state.envelope = attackCoeff * state.envelope + (1.0f - attackCoeff) * std::abs (processed);
            rms += state.envelope * state.envelope;

            state.lookaheadLine.pushSample (0, in);
        }
        rms = std::sqrt (rms / juce::jmax (1, numChannels));

        float gain = 1.0f;
        if (rms > 0.0f)
        {
            const float desired = targetGain / rms;
            const float rangeGain = juce::Decibels::decibelsToGain (rangeDb);
            gain = juce::jlimit (1.0f / rangeGain, rangeGain, desired);
        }

        if (gain < gainSmoothed)
            gainSmoothed = attackCoeff * (gainSmoothed - gain) + gain;
        else
            gainSmoothed = releaseCoeff * (gainSmoothed - gain) + gain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            float delayed = state.lookaheadLine.popSample (0);
            buffer.setSample (ch, sample, delayed * gainSmoothed * outputTrim);
        }
    }
}

void DYNRMSRiderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void DYNRMSRiderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNRMSRiderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("target_level", "Target Level",
                                                                   juce::NormalisableRange<float> (-30.0f, -3.0f, 0.1f), -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("speed",        "Speed",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("range",        "Range",
                                                                   juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hf_sensitivity","HF Sensitivity",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lookahead",    "Lookahead",
                                                                   juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,   "Input Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput,  "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass,  "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNRMSRiderAudioProcessorEditor::DYNRMSRiderAudioProcessorEditor (DYNRMSRiderAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.RMSRider", "RMS Rider")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false) { initSlider (slider, label, macro); };

    make (targetLevelSlider,   "Target", true);
    make (speedSlider,         "Speed", true);
    make (rangeSlider,         "Range");
    make (hfSensitivitySlider, "HF Sens");
    make (lookaheadSlider,     "Lookahead");
    make (inputTrimSlider,     "Input");
    make (outputTrimSlider,    "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "target_level", "speed", "range", "hf_sensitivity", "lookahead", kParamInput, kParamOutput };
    juce::Slider* sliders[]      = { &targetLevelSlider, &speedSlider, &rangeSlider, &hfSensitivitySlider, &lookaheadSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (820, 420);
}

void DYNRMSRiderAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto label = std::make_unique<juce::Label>();
    label->setText (name, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    labels.push_back (std::move (label));
}

void DYNRMSRiderAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNRMSRiderAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &targetLevelSlider, &speedSlider, &rangeSlider, &hfSensitivitySlider, &lookaheadSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void DYNRMSRiderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNRMSRiderAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto topRow = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottomRow = area;

    auto width = topRow.getWidth() / 3;
    targetLevelSlider.setBounds (topRow.removeFromLeft (width).reduced (8));
    speedSlider     .setBounds (topRow.removeFromLeft (width).reduced (8));
    rangeSlider     .setBounds (topRow.removeFromLeft (width).reduced (8));

    auto bottomWidth = bottomRow.getWidth() / 4;
    hfSensitivitySlider.setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    lookaheadSlider    .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider    .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider   .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* DYNRMSRiderAudioProcessor::createEditor()
{
    return new DYNRMSRiderAudioProcessorEditor (*this);
}

int DYNRMSRiderAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNRMSRiderAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNRMSRiderAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNRMSRiderAudioProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return;

    const auto& preset = presetBank[(size_t) index];
    for (const auto& entry : preset.params)
    {
        if (auto* param = apvts.getParameter (entry.first))
        {
            auto norm = param->getNormalisableRange().convertTo0to1 (entry.second);
            param->setValueNotifyingHost (norm);
        }
    }
}

void DYNRMSRiderAudioProcessor::ensureStateSize (int requiredChannels)
{
    requiredChannels = juce::jmax (0, requiredChannels);

    if (static_cast<int> (channelStates.size()) != requiredChannels)
    {
        channelStates.assign ((size_t) requiredChannels, ChannelState {});

        if (requiredChannels > 0 && currentSampleRate > 0.0)
        {
            const auto blockSize = lastBlockSize == 0 ? 512u : lastBlockSize;
            juce::dsp::ProcessSpec spec { currentSampleRate, blockSize, 1 };

            for (auto& state : channelStates)
            {
                state.lookaheadLine.prepare (spec);
                state.lookaheadLine.reset();
                state.envelope = 0.0f;
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DYNRMSRiderAudioProcessor();
}
