#include "EQBusPaintAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "BUS_PAINT";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<EQBusPaintAudioProcessor::Preset, 3> EQBusPaintAudioProcessor::presetBank {{
    { "Drum Bus", {
        { "low_tilt",     1.5f },
        { "high_tilt",    1.0f },
        { "presence",     2.0f },
        { "warmth",      -1.5f },
        { kParamInput,    0.0f },
        { kParamOutput,   0.0f },
        { kParamBypass,   0.0f }
    }},
    { "Mix Paint", {
        { "low_tilt",     0.8f },
        { "high_tilt",    1.2f },
        { "presence",     0.5f },
        { "warmth",       0.3f },
        { kParamInput,    0.0f },
        { kParamOutput,   0.0f },
        { kParamBypass,   0.0f }
    }},
    { "Instrument Glue", {
        { "low_tilt",    -0.8f },
        { "high_tilt",    1.0f },
        { "presence",    -0.5f },
        { "warmth",       1.2f },
        { kParamInput,    0.0f },
        { kParamOutput,  -0.5f },
        { kParamBypass,   0.0f }
    }}
}};

EQBusPaintAudioProcessor::EQBusPaintAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void EQBusPaintAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureFilterState (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    auto prepareVector = [&](auto& vec)
    {
        for (auto& filter : vec)
        {
            filter.prepare (spec);
            filter.reset();
        }
    };

    prepareVector (lowShelves);
    prepareVector (highShelves);
    prepareVector (presenceBells);
    prepareVector (warmthBells);
}

void EQBusPaintAudioProcessor::releaseResources()
{
}

void EQBusPaintAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float lowTilt    = get ("low_tilt");
    const float highTilt   = get ("high_tilt");
    const float presence   = get ("presence");
    const float warmth     = get ("warmth");
    const float inputGain  = juce::Decibels::decibelsToGain (get (kParamInput));
    const float outputGain = juce::Decibels::decibelsToGain (get (kParamOutput));
    const bool bypassed    = get (kParamBypass) > 0.5f;

    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());
    ensureFilterState (buffer.getNumChannels());
    updateFilters (lowTilt, highTilt, presence, warmth);

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock (ch);
        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
        lowShelves[ch].process (ctx);
        highShelves[ch].process (ctx);
        presenceBells[ch].process (ctx);
        warmthBells[ch].process (ctx);
    }

    buffer.applyGain (outputGain);
}

void EQBusPaintAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void EQBusPaintAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQBusPaintAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_tilt",  "Low Tilt",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_tilt", "High Tilt",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence",  "Presence",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("warmth",    "Warmth",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput, "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput,"Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass,"Soft Bypass", false));

    return { params.begin(), params.end() };
}

EQBusPaintAudioProcessorEditor::EQBusPaintAudioProcessorEditor (EQBusPaintAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("EQ")),
      headerComponent ("EQ.BusPaint", "Bus Paint")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& s, const juce::String& l, bool macro = false) { initSlider (s, l, macro); };
    make (lowTiltSlider,    "Low Tilt", true);
    make (highTiltSlider,   "High Tilt", true);
    make (presenceSlider,   "Presence");
    make (warmthSlider,     "Warmth");
    make (inputTrimSlider,  "Input");
    make (outputTrimSlider, "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "low_tilt", "high_tilt", "presence", "warmth", kParamInput, kParamOutput };
    juce::Slider* sliders[]     = { &lowTiltSlider, &highTiltSlider, &presenceSlider, &warmthSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (760, 420);
}

void EQBusPaintAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void EQBusPaintAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void EQBusPaintAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &lowTiltSlider, &highTiltSlider, &presenceSlider, &warmthSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* slider = sliders[i])
            labels[i]->setBounds (slider->getBounds().withHeight (18).translated (0, -20));
    }
}

void EQBusPaintAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void EQBusPaintAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    const int topWidth = top.getWidth() / 4;
    lowTiltSlider   .setBounds (top.removeFromLeft (topWidth).reduced (8));
    highTiltSlider  .setBounds (top.removeFromLeft (topWidth).reduced (8));
    presenceSlider  .setBounds (top.removeFromLeft (topWidth).reduced (8));
    warmthSlider    .setBounds (top.removeFromLeft (topWidth).reduced (8));

    const int bottomWidth = bottom.getWidth() / 3;
    inputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* EQBusPaintAudioProcessor::createEditor()
{
    return new EQBusPaintAudioProcessorEditor (*this);
}

int EQBusPaintAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String EQBusPaintAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void EQBusPaintAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void EQBusPaintAudioProcessor::ensureFilterState (int numChannels)
{
    if (numChannels <= 0)
    {
        lowShelves.clear();
        highShelves.clear();
        presenceBells.clear();
        warmthBells.clear();
        return;
    }

    auto ensureVector = [&](auto& vec)
    {
        if ((int) vec.size() < numChannels)
        {
            juce::dsp::ProcessSpec spec { currentSampleRate,
                                          lastBlockSize > 0 ? lastBlockSize : 512u,
                                          1 };
            const auto previous = (int) vec.size();
            vec.resize ((size_t) numChannels);
            for (int ch = previous; ch < numChannels; ++ch)
            {
                vec[(size_t) ch].prepare (spec);
                vec[(size_t) ch].reset();
            }
        }
    };

    ensureVector (lowShelves);
    ensureVector (highShelves);
    ensureVector (presenceBells);
    ensureVector (warmthBells);
}

void EQBusPaintAudioProcessor::updateFilters (float lowTilt, float highTilt, float presence, float warmth)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lowShelfGain  = juce::Decibels::decibelsToGain (lowTilt);
    const auto highShelfGain = juce::Decibels::decibelsToGain (highTilt);
    const auto presenceGain  = juce::Decibels::decibelsToGain (presence);
    const auto warmthGain    = juce::Decibels::decibelsToGain (warmth);

    constexpr float lowShelfFreq  = 150.0f;
    constexpr float highShelfFreq = 6000.0f;
    constexpr float presenceFreq  = 3200.0f;
    constexpr float warmthFreq    = 450.0f;

    auto lowCoeffs     = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, lowShelfFreq, 0.707f, lowShelfGain);
    auto highCoeffs    = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, highShelfFreq, 0.707f, highShelfGain);
    auto presenceCoeffs= juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, presenceFreq, 1.0f, presenceGain);
    auto warmthCoeffs  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, warmthFreq, 0.8f, warmthGain);

    for (auto& filter : lowShelves)
        filter.coefficients = lowCoeffs;
    for (auto& filter : highShelves)
        filter.coefficients = highCoeffs;
    for (auto& filter : presenceBells)
        filter.coefficients = presenceCoeffs;
    for (auto& filter : warmthBells)
        filter.coefficients = warmthCoeffs;
}

void EQBusPaintAudioProcessor::applyPreset (int index)
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQBusPaintAudioProcessor();
}
