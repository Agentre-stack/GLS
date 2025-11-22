#include "DYNBusLiftAudioProcessor.h"

namespace
{
constexpr auto kStateId = "BUS_LIFT";
}

const std::array<DYNBusLiftAudioProcessor::Preset, 3> DYNBusLiftAudioProcessor::presetBank {{
    { "Drum Bus", {
        { "low_thresh",  -18.0f },
        { "mid_thresh",  -14.0f },
        { "high_thresh", -10.0f },
        { "ratio",         3.0f },
        { "attack",        8.0f },
        { "release",     150.0f },
        { "mix",           0.85f },
        { "input_trim",    0.0f },
        { "output_trim",   0.0f },
        { "ui_bypass",     0.0f }
    }},
    { "Mix Glue", {
        { "low_thresh",  -16.0f },
        { "mid_thresh",  -12.0f },
        { "high_thresh",  -8.0f },
        { "ratio",         2.2f },
        { "attack",       12.0f },
        { "release",     220.0f },
        { "mix",           0.7f },
        { "input_trim",    0.0f },
        { "output_trim",   0.5f },
        { "ui_bypass",     0.0f }
    }},
    { "Vocal Lift", {
        { "low_thresh",  -22.0f },
        { "mid_thresh",  -18.0f },
        { "high_thresh", -15.0f },
        { "ratio",         2.8f },
        { "attack",        6.0f },
        { "release",     130.0f },
        { "mix",           0.9f },
        { "input_trim",   -1.0f },
        { "output_trim",   0.0f },
        { "ui_bypass",     0.0f }
    }}
}};

DYNBusLiftAudioProcessor::DYNBusLiftAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void DYNBusLiftAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    auto resetFilter = [sampleRate](auto& filter, juce::dsp::LinkwitzRileyFilterType type, float freq)
    {
        filter.setType (type);
        filter.setCutoffFrequency (freq);
        filter.reset();
        juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
        filter.prepare (spec);
    };

    resetFilter (lowLowpass,  juce::dsp::LinkwitzRileyFilterType::lowpass, 200.0f);
    resetFilter (lowHighpass, juce::dsp::LinkwitzRileyFilterType::highpass, 200.0f);
    resetFilter (midLowpass,  juce::dsp::LinkwitzRileyFilterType::lowpass, 2000.0f);
    resetFilter (midHighpass, juce::dsp::LinkwitzRileyFilterType::highpass, 200.0f);
    resetFilter (highLowpass, juce::dsp::LinkwitzRileyFilterType::lowpass, 20000.0f);
    resetFilter (highHighpass, juce::dsp::LinkwitzRileyFilterType::highpass, 2000.0f);
}

void DYNBusLiftAudioProcessor::releaseResources()
{
}

void DYNBusLiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const bool bypassed   = apvts.getRawParameterValue ("ui_bypass")->load() > 0.5f;
    const auto inputTrim  = juce::Decibels::decibelsToGain (read ("input_trim"));
    const auto lowThresh  = read ("low_thresh");
    const auto midThresh  = read ("mid_thresh");
    const auto highThresh = read ("high_thresh");
    const auto ratio      = juce::jmax (1.0f, read ("ratio"));
    const auto attack     = read ("attack");
    const auto release    = read ("release");
    const auto mix        = juce::jlimit (0.0f, 1.0f, read ("mix"));
    const auto outputTrim = juce::Decibels::decibelsToGain (read ("output_trim"));

    if (bypassed)
        return;

    buffer.applyGain (inputTrim);
    dryBuffer.makeCopyOf (buffer, true);
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    lowBuffer.setSize (numChannels, numSamples, false, false, true);
    midBuffer.setSize (numChannels, numSamples, false, false, true);
    highBuffer.setSize (numChannels, numSamples, false, false, true);

    lowBuffer.makeCopyOf (buffer);
    midBuffer.makeCopyOf (buffer);
    highBuffer.makeCopyOf (buffer);

    juce::dsp::AudioBlock<float> lowBlock (lowBuffer);
    juce::dsp::AudioBlock<float> midBlock (midBuffer);
    juce::dsp::AudioBlock<float> highBlock (highBuffer);
    lowLowpass.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    lowHighpass.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    midLowpass.process (juce::dsp::ProcessContextReplacing<float> (midBlock));
    midHighpass.process (juce::dsp::ProcessContextReplacing<float> (midBlock));
    highHighpass.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

    processBand (lowBuffer, lowThresh, ratio, attack, release);
    processBand (midBuffer, midThresh, ratio, attack, release);
    processBand (highBuffer, highThresh, ratio, attack, release);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        const auto* low = lowBuffer.getReadPointer (ch);
        const auto* mid = midBuffer.getReadPointer (ch);
        const auto* high = highBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float processed = low[i] + mid[i] + high[i];
            out[i] = processed * mix + dry[i] * (1.0f - mix);
        }
    }

    if (outputTrim != 1.0f)
        buffer.applyGain (outputTrim);
}

void DYNBusLiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void DYNBusLiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNBusLiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto threshRange = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto timeRange   = juce::NormalisableRange<float> (1.0f, 100.0f, 0.01f, 0.35f);
    auto releaseRange= juce::NormalisableRange<float> (10.0f, 600.0f, 0.01f, 0.35f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim", "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_thresh",  "Low Thresh",  threshRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mid_thresh",  "Mid Thresh",  threshRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_thresh", "High Thresh", threshRange, -12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",       "Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f, 0.5f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",      "Attack", timeRange, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",     "Release", releaseRange, 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",         "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("ui_bypass", "Soft Bypass", false));

    return { params.begin(), params.end() };
}

DYNBusLiftAudioProcessorEditor::DYNBusLiftAudioProcessorEditor (DYNBusLiftAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("DYN")),
      headerComponent ("DYN.BusLift", "Bus Lift")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto init = [this](juce::Slider& slider, const juce::String& label, bool macro = false) { initSlider (slider, label, macro); };

    init (lowThreshSlider,  "Low Thresh", true);
    init (midThreshSlider,  "Mid Thresh", true);
    init (highThreshSlider, "High Thresh", true);
    init (ratioSlider,      "Ratio", true);
    init (attackSlider,     "Attack");
    init (releaseSlider,    "Release");
    init (mixSlider,        "Mix");
    init (inputTrimSlider,  "Input");
    init (outputTrimSlider, "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "low_thresh", "mid_thresh", "high_thresh", "ratio", "attack", "release", "mix", "input_trim", "output_trim" };
    juce::Slider* sliders[] = { &lowThreshSlider, &midThreshSlider, &highThreshSlider,
                                &ratioSlider, &attackSlider, &releaseSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "ui_bypass", bypassButton));

    setSize (880, 420);
}

void DYNBusLiftAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool isMacro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isMacro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto label = std::make_unique<juce::Label>();
    label->setText (name, juce::dontSendNotification);
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    label->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*label);
    sliderLabels.push_back (std::move (label));
}

void DYNBusLiftAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void DYNBusLiftAudioProcessorEditor::layoutLabels()
{
    const std::array<juce::Slider*, 9> sliders { &lowThreshSlider, &midThreshSlider, &highThreshSlider,
                                                 &ratioSlider, &attackSlider, &releaseSlider, &mixSlider,
                                                 &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < sliderLabels.size(); ++i)
    {
        auto* slider = sliders[i];
        auto* label = sliderLabels[i].get();
        if (slider != nullptr && label != nullptr)
        {
            auto bounds = slider->getBounds().withHeight (18).translated (0, -20);
            label->setBounds (bounds);
        }
    }
}

void DYNBusLiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.25f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void DYNBusLiftAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto body = bounds.reduced (12);
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * 0.55f)).reduced (10);
    auto right = body.reduced (10);

    auto macroHeight = left.getHeight() / 2;
    auto macroRow = left.removeFromTop (macroHeight);
    auto macroWidth = macroRow.getWidth() / 4;
    lowThreshSlider .setBounds (macroRow.removeFromLeft (macroWidth).reduced (6));
    midThreshSlider .setBounds (macroRow.removeFromLeft (macroWidth).reduced (6));
    highThreshSlider.setBounds (macroRow.removeFromLeft (macroWidth).reduced (6));
    ratioSlider     .setBounds (macroRow.removeFromLeft (macroWidth).reduced (6));

    auto microRow = left;
    auto microWidth = microRow.getWidth() / 3;
    attackSlider .setBounds (microRow.removeFromLeft (microWidth).reduced (6));
    releaseSlider.setBounds (microRow.removeFromLeft (microWidth).reduced (6));
    mixSlider    .setBounds (microRow.removeFromLeft (microWidth).reduced (6));

    auto rightHeight = right.getHeight() / 2;
    inputTrimSlider .setBounds (right.removeFromTop (rightHeight).reduced (8));
    outputTrimSlider.setBounds (right.removeFromTop (rightHeight).reduced (8));
    bypassButton    .setBounds (right.removeFromTop (32).reduced (4));

    layoutLabels();
}

juce::AudioProcessorEditor* DYNBusLiftAudioProcessor::createEditor()
{
    return new DYNBusLiftAudioProcessorEditor (*this);
}

void DYNBusLiftAudioProcessor::processBand (juce::AudioBuffer<float>& bandBuffer, float thresholdDb, float ratio,
                                            float attackMs, float releaseMs)
{
    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));
    const auto thresholdGain = juce::Decibels::decibelsToGain (thresholdDb);

    for (int ch = 0; ch < bandBuffer.getNumChannels(); ++ch)
    {
        float envelope = 0.0f;
        auto* data = bandBuffer.getWritePointer (ch);
        for (int i = 0; i < bandBuffer.getNumSamples(); ++i)
        {
            const float sample = data[i];
            const float level = std::abs (sample);
            if (level > envelope)
                envelope = attackCoeff * envelope + (1.0f - attackCoeff) * level;
            else
                envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * level;

            float gain = 1.0f;
            if (envelope > thresholdGain)
            {
                const auto envDb = juce::Decibels::gainToDecibels (envelope);
                const auto compressed = thresholdDb + (envDb - thresholdDb) / ratio;
                gain = juce::Decibels::decibelsToGain (compressed - envDb);
            }

            data[i] *= gain;
        }
    }
}

int DYNBusLiftAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String DYNBusLiftAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void DYNBusLiftAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void DYNBusLiftAudioProcessor::applyPreset (int index)
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
    return new DYNBusLiftAudioProcessor();
}
