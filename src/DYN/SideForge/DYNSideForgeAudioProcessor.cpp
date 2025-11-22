#include "DYNSideForgeAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "SIDE_FORGE";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<DYNSideForgeAudioProcessor::Preset, 3> DYNSideForgeAudioProcessor::presetBank {{
    { "Bus Glue", {
        { "thresh",      -16.0f },
        { "ratio",         2.5f },
        { "attack",       15.0f },
        { "release",     180.0f },
        { "sc_hpf",       80.0f },
        { "sc_lpf",     12000.0f },
        { "lookahead",     5.0f },
        { "mix",           0.75f },
        { kParamInput,     0.0f },
        { kParamOutput,    0.0f },
        { kParamBypass,    0.0f }
    }},
    { "Drum Side", {
        { "thresh",      -20.0f },
        { "ratio",         3.5f },
        { "attack",       10.0f },
        { "release",     140.0f },
        { "sc_hpf",       90.0f },
        { "sc_lpf",      8000.0f },
        { "lookahead",     6.0f },
        { "mix",           0.8f },
        { kParamInput,     0.0f },
        { kParamOutput,   -0.5f },
        { kParamBypass,    0.0f }
    }},
    { "Vocal Tight", {
        { "thresh",      -18.0f },
        { "ratio",         2.2f },
        { "attack",       12.0f },
        { "release",     200.0f },
        { "sc_hpf",      120.0f },
        { "sc_lpf",      7000.0f },
        { "lookahead",     5.0f },
        { "mix",           0.7f },
        { kParamInput,    -1.0f },
        { kParamOutput,    0.0f },
        { kParamBypass,    0.0f }
    }}
}};

DYNSideForgeAudioProcessor::DYNSideForgeAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNSideForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize();
    gainSmoothed = 1.0f;
}

void DYNSideForgeAudioProcessor::releaseResources()
{
}

void DYNSideForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb  = read ("thresh");
    const auto ratio     = juce::jmax (1.0f, read ("ratio"));
    const auto attackMs  = read ("attack");
    const auto releaseMs = read ("release");
    const auto scHpf     = read ("sc_hpf");
    const auto scLpf     = read ("sc_lpf");
    const auto lookahead = read ("lookahead");
    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto inputGain = juce::Decibels::decibelsToGain (read (kParamInput));
    const auto outputGain= juce::Decibels::decibelsToGain (read (kParamOutput));
    const bool bypassed  = read (kParamBypass) > 0.5f;

    ensureStateSize();
    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    dryBuffer.makeCopyOf (buffer, true);

    scHpfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, scHpf);
    scLpfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, scLpf);

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto thresholdGain= juce::Decibels::decibelsToGain (threshDb);

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const auto delaySamples = juce::roundToInt (lookahead * 0.001f * currentSampleRate);

    for (auto& state : channelStates)
        state.lookahead.setDelay (delaySamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float scSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            const float in = buffer.getSample (ch, sample);
            state.lookahead.pushSample (0, in);
            scSample += in * 0.5f;
        }

        scSample = scHpfFilter.processSample (scSample);
        scSample = scLpfFilter.processSample (scSample);
        scSample = std::abs (scSample);

        float targetGain = 1.0f;
        if (scSample > thresholdGain)
        {
            const auto over = juce::Decibels::gainToDecibels (scSample) - threshDb;
            const auto compressed = threshDb + over / ratio;
            const auto gainDb = compressed - juce::Decibels::gainToDecibels (scSample);
            targetGain = juce::Decibels::decibelsToGain (gainDb);
        }

        if (targetGain < gainSmoothed)
            gainSmoothed = attackCoeff * (gainSmoothed - targetGain) + targetGain;
        else
            gainSmoothed = releaseCoeff * (gainSmoothed - targetGain) + targetGain;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channelStates[ch];
            float delayed = state.lookahead.popSample (0);
            buffer.setSample (ch, sample, delayed * gainSmoothed * outputGain);
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }
}

void DYNSideForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void DYNSideForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNSideForgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",  "Threshold",
                                                                   juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",   "Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",  "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 1000.0f, 0.01f, 0.35f), 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sc_hpf",  "SC HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 400.0f, 0.01f, 0.35f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sc_lpf",  "SC LPF",
                                                                   juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.01f, 0.35f), 6000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lookahead","Lookahead",
                                                                   juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.35f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",     "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput, "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNSideForgeAudioProcessorEditor::DYNSideForgeAudioProcessorEditor (DYNSideForgeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.SideForge", "Side Forge")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false) { initSlider (slider, label, macro); };

    make (threshSlider,   "Thresh", true);
    make (ratioSlider,    "Ratio", true);
    make (attackSlider,   "Attack");
    make (releaseSlider,  "Release");
    make (scHpfSlider,    "SC HPF");
    make (scLpfSlider,    "SC LPF");
    make (lookaheadSlider,"Lookahead");
    make (mixSlider,      "Mix");
    make (inputTrimSlider,"Input");
    make (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "thresh", "ratio", "attack", "release", "sc_hpf", "sc_lpf", "lookahead", "mix", kParamInput, kParamOutput };
    juce::Slider* sliders[]      = { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                     &scHpfSlider, &scLpfSlider, &lookaheadSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (880, 420);
}

void DYNSideForgeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
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

void DYNSideForgeAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNSideForgeAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
        &scHpfSlider, &scLpfSlider, &lookaheadSlider, &mixSlider,
        &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void DYNSideForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNSideForgeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto width = top.getWidth() / 4;
    threshSlider .setBounds (top.removeFromLeft (width).reduced (8));
    ratioSlider  .setBounds (top.removeFromLeft (width).reduced (8));
    attackSlider .setBounds (top.removeFromLeft (width).reduced (8));
    releaseSlider.setBounds (top.removeFromLeft (width).reduced (8));

    auto bottomWidth = bottom.getWidth() / 5;
    scHpfSlider      .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    scLpfSlider      .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    lookaheadSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    mixSlider        .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider  .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    outputTrimSlider.setBounds (footerComponent.getBounds().withSizeKeepingCentre (120, 48));
    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));

    layoutLabels();
}

juce::AudioProcessorEditor* DYNSideForgeAudioProcessor::createEditor()
{
    return new DYNSideForgeAudioProcessorEditor (*this);
}

int DYNSideForgeAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNSideForgeAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNSideForgeAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNSideForgeAudioProcessor::applyPreset (int index)
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

void DYNSideForgeAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
    {
        channelStates.resize (requiredChannels);
    }

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };
    scHpfFilter.prepare (spec);
    scHpfFilter.reset();
    scLpfFilter.prepare (spec);
    scLpfFilter.reset();
    for (auto& state : channelStates)
    {
        state.lookahead.prepare (spec);
        state.lookahead.reset();
        state.envelope = 0.0f;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DYNSideForgeAudioProcessor();
}
