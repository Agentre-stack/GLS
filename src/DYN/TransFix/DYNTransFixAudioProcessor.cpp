#include "DYNTransFixAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "TRANS_FIX";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<DYNTransFixAudioProcessor::Preset, 3> DYNTransFixAudioProcessor::presetBank {{
    { "Drum Snap", {
        { "attack",       4.0f },
        { "sustain",     -2.0f },
        { "tilt_freq",  2500.0f },
        { "tilt_amount",  0.25f },
        { "detect_mode",   1.0f }, // HF focus
        { "mix",           0.85f },
        { kParamInput,     0.0f },
        { kParamOutput,    0.0f },
        { kParamBypass,    0.0f }
    }},
    { "Vocal Pop", {
        { "attack",       2.5f },
        { "sustain",     -3.0f },
        { "tilt_freq",  1800.0f },
        { "tilt_amount",  0.18f },
        { "detect_mode",   1.0f },
        { "mix",           0.9f },
        { kParamInput,     0.0f },
        { kParamOutput,    0.5f },
        { kParamBypass,    0.0f }
    }},
    { "Bus Smooth", {
        { "attack",      -1.0f },
        { "sustain",      2.0f },
        { "tilt_freq",  1200.0f },
        { "tilt_amount", -0.1f },
        { "detect_mode",  0.0f },
        { "mix",          0.8f },
        { kParamInput,    0.0f },
        { kParamOutput,   0.0f },
        { kParamBypass,   0.0f }
    }}
}};

DYNTransFixAudioProcessor::DYNTransFixAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNTransFixAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize();
    for (auto& state : channelStates)
    {
        state.detector = 0.0f;
        state.attackEnv = 0.0f;
        state.sustainEnv = 0.0f;
        state.hfFilter.reset();
        state.lfFilter.reset();
    }
}

void DYNTransFixAudioProcessor::releaseResources()
{
}

void DYNTransFixAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto attackGain   = juce::Decibels::decibelsToGain (read ("attack"));
    const auto sustainGain  = juce::Decibels::decibelsToGain (read ("sustain"));
    const auto tiltFreq     = read ("tilt_freq");
    const auto tiltAmount   = read ("tilt_amount");
    const int detectMode    = static_cast<int> (apvts.getRawParameterValue ("detect_mode")->load());
    const auto mix          = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputGain    = juce::Decibels::decibelsToGain (read (kParamInput));
    const auto outputGain   = juce::Decibels::decibelsToGain (read (kParamOutput));
    const bool bypassed     = read (kParamBypass) > 0.5f;

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureStateSize();
    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    dryBuffer.makeCopyOf (buffer);

    const auto attackCoeff  = std::exp (-1.0f / (0.001f * currentSampleRate));
    const auto sustainCoeff = std::exp (-1.0f / (0.01f * currentSampleRate));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            float detectorSample = sample;

            if (detectMode == 1) // HF focus
                detectorSample = state.hfFilter.processSample (sample);
            else if (detectMode == 2) // LF focus
                detectorSample = state.lfFilter.processSample (sample);

            const float level = std::abs (detectorSample);
            if (level > state.detector)
                state.detector = attackCoeff * state.detector + (1.0f - attackCoeff) * level;
            else
                state.detector = sustainCoeff * state.detector + (1.0f - sustainCoeff) * level;

            const float attackMultiplier  = 1.0f + (attackGain - 1.0f) * juce::jlimit (0.0f, 1.0f, state.detector * 2.0f);
            const float sustainMultiplier = 1.0f + (sustainGain - 1.0f) * (1.0f - juce::jlimit (0.0f, 1.0f, state.detector * 2.0f));

            sample *= attackMultiplier * sustainMultiplier;
            sample = applyTilt (sample, tiltFreq, tiltAmount);
            data[i] = sample;
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = (wet[i] * mix + dry[i] * (1.0f - mix)) * outputGain;
    }
}

void DYNTransFixAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void DYNTransFixAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNTransFixAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",     "Attack",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sustain",    "Sustain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt_freq",  "Tilt Freq",
                                                                   juce::NormalisableRange<float> (100.0f, 8000.0f, 0.01f, 0.4f), 1200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt_amount","Tilt Amount",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("detect_mode","Detect Mode",
                                                                    juce::StringArray { "Wideband", "HF Focus", "LF Focus" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,  "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNTransFixAudioProcessorEditor::DYNTransFixAudioProcessorEditor (DYNTransFixAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.TransFix", "Trans Fix")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto init = [this](juce::Slider& s, const juce::String& l, bool macro = false) { initSlider (s, l, macro); };

    init (attackSlider,     "Attack", true);
    init (sustainSlider,    "Sustain", true);
    init (tiltFreqSlider,   "Tilt Freq");
    init (tiltAmountSlider, "Tilt Amt");
    init (mixSlider,        "Mix");
    init (inputTrimSlider,  "Input");
    init (outputTrimSlider, "Output");
    initToggle (bypassButton);

    detectModeBox.addItemList ({ "Wideband", "HF Focus", "LF Focus" }, 1);
    detectModeBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detectModeBox);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "attack", "sustain", "tilt_freq", "tilt_amount", "mix", kParamInput, kParamOutput };
    juce::Slider* sliders[]     = { &attackSlider, &sustainSlider, &tiltFreqSlider, &tiltAmountSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    detectModeAttachment = std::make_unique<ComboBoxAttachment> (state, "detect_mode", detectModeBox);
    buttonAttachment = std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton);

    setSize (780, 420);
}

void DYNTransFixAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void DYNTransFixAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNTransFixAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &attackSlider, &sustainSlider, &tiltFreqSlider, &tiltAmountSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void DYNTransFixAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNTransFixAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.6f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 4;
    attackSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    sustainSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));
    tiltFreqSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));
    tiltAmountSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 4;
    mixSlider       .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    detectModeBox   .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8).removeFromTop (48));
    inputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* DYNTransFixAudioProcessor::createEditor()
{
    return new DYNTransFixAudioProcessorEditor (*this);
}

void DYNTransFixAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = juce::jmax (0, getTotalNumOutputChannels());
    if (requiredChannels <= 0)
    {
        channelStates.clear();
        dryBuffer.setSize (0, 0);
        return;
    }

    channelStates.resize ((size_t) requiredChannels);

    juce::dsp::ProcessSpec spec {
        currentSampleRate > 0.0 ? currentSampleRate : 44100.0,
        lastBlockSize > 0 ? lastBlockSize : 512u,
        1
    };

    for (auto& state : channelStates)
    {
        state.hfFilter.prepare (spec);
        state.lfFilter.prepare (spec);
        state.hfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 2000.0f);
        state.lfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, 500.0f);
    }

    dryBuffer.setSize (requiredChannels, (int) (lastBlockSize > 0 ? lastBlockSize : 512u), false, false, true);
}

float DYNTransFixAudioProcessor::applyTilt (float sample, float freq, float amount)
{
    const float pivot = juce::jlimit (0.0f, 1.0f, (float) (freq / currentSampleRate));
    const float tilt = amount * 0.5f;
    const float lowGain = 1.0f + tilt;
    const float highGain = 1.0f - tilt;
    return sample * (pivot * highGain + (1.0f - pivot) * lowGain);
}

int DYNTransFixAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNTransFixAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNTransFixAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNTransFixAudioProcessor::applyPreset (int index)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DYNTransFixAudioProcessor();
}
