#include "GRDWarmLiftAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "WARM_LIFT";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
constexpr auto kParamBypass = "ui_bypass";
}

const std::array<GRDWarmLiftAudioProcessor::Preset, 3> GRDWarmLiftAudioProcessor::presetBank {{
    { "Vocal Warm Lift", {
        { "warmth",      3.5f },
        { "shine",       2.5f },
        { "drive",       0.45f },
        { "tighten",    90.0f },
        { "mix",         0.7f },
        { kParamInput,   0.0f },
        { kParamOutput, -0.5f },
        { kParamBypass,  0.0f }
    }},
    { "Guitar Glow", {
        { "warmth",      4.5f },
        { "shine",       1.5f },
        { "drive",       0.5f },
        { "tighten",   120.0f },
        { "mix",         0.65f },
        { kParamInput,   0.0f },
        { kParamOutput, -1.0f },
        { kParamBypass,  0.0f }
    }},
    { "Bus Glue", {
        { "warmth",      2.0f },
        { "shine",       3.0f },
        { "drive",       0.35f },
        { "tighten",   150.0f },
        { "mix",         0.55f },
        { kParamInput,   0.0f },
        { kParamOutput,  0.0f },
        { kParamBypass,  0.0f }
    }}
}};

GRDWarmLiftAudioProcessor::GRDWarmLiftAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDWarmLiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
}

void GRDWarmLiftAudioProcessor::releaseResources()
{
}

void GRDWarmLiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const float warmth  = juce::jlimit (-12.0f, 12.0f, get ("warmth"));
    const float shine   = juce::jlimit (-12.0f, 12.0f, get ("shine"));
    const float drive   = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float tighten = juce::jlimit (20.0f, 220.0f, get ("tighten"));
    const float mix     = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float inputGain  = juce::Decibels::decibelsToGain (juce::jlimit (-18.0f, 18.0f, get (kParamInput)));
    const float trim    = juce::Decibels::decibelsToGain (juce::jlimit (-18.0f, 18.0f, get (kParamOutput)));
    const bool bypassed = get (kParamBypass) > 0.5f;

    ensureStateSize (numChannels);
    updateFilters (warmth, shine, tighten);

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer, true);

    buffer.applyGain (inputGain);
    if (bypassed)
    {
        buffer.applyGain (trim);
        return;
    }

    const float driveGain = 1.0f + drive * 6.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = dry.getSample (ch, i);
            float sample = state.tightenFilter.processSample (input);
            sample = state.warmthShelf.processSample (sample);
            sample = state.shineShelf.processSample (sample);
            const float shaped = std::tanh (sample * driveGain);
            data[i] = juce::jmap (mix, input, shaped) * trim;
        }
    }
}

void GRDWarmLiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDWarmLiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDWarmLiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("warmth", "Warmth",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("shine", "Shine",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tighten", "Tighten",
                                                                   juce::NormalisableRange<float> (20.0f, 220.0f, 0.01f, 0.35f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput, "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GRDWarmLiftAudioProcessor::ensureStateSize (int numChannels)
{
    if ((int) channelState.size() < numChannels)
        channelState.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (filterSpecSampleRate, currentSampleRate)
                             || filterSpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& state : channelState)
        {
            state.warmthShelf.prepare (spec);
            state.warmthShelf.reset();
            state.shineShelf.prepare (spec);
            state.shineShelf.reset();
            state.tightenFilter.prepare (spec);
            state.tightenFilter.reset();
        }
        filterSpecSampleRate = currentSampleRate;
        filterSpecBlockSize  = targetBlock;
    }
}

void GRDWarmLiftAudioProcessor::updateFilters (float warmth, float shine, float tighten)
{
    if (currentSampleRate <= 0.0)
        return;

    const float warmthGain = juce::Decibels::decibelsToGain (warmth);
    const float shineGain  = juce::Decibels::decibelsToGain (shine);
    auto warmthCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate,
                                                                           180.0f,
                                                                           0.7f,
                                                                           warmthGain);
    auto shineCoeffs  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate,
                                                                            4800.0f,
                                                                            0.8f,
                                                                            shineGain);
    auto tightenCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                            juce::jlimit (20.0f, 300.0f, tighten), 0.7f);

    for (auto& state : channelState)
    {
        state.warmthShelf.coefficients = warmthCoeffs;
        state.shineShelf.coefficients  = shineCoeffs;
        state.tightenFilter.coefficients = tightenCoeffs;
    }
}

int GRDWarmLiftAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDWarmLiftAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDWarmLiftAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDWarmLiftAudioProcessor::applyPreset (int index)
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

juce::AudioProcessorEditor* GRDWarmLiftAudioProcessor::createEditor()
{
    return new GRDWarmLiftAudioProcessorEditor (*this);
}

//==============================================================================
GRDWarmLiftAudioProcessorEditor::GRDWarmLiftAudioProcessorEditor (GRDWarmLiftAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.WarmLift", "Warm Lift")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false) { initSlider (slider, label, macro); };
    make (warmthSlider, "Warmth", true);
    make (shineSlider,  "Shine", true);
    make (driveSlider,  "Drive");
    make (tightenSlider,"Tighten");
    make (mixSlider,    "Mix");
    make (inputTrimSlider, "Input");
    make (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "warmth", "shine", "drive", "tighten", "mix", kParamInput, kParamOutput };
    juce::Slider* sliders[] = { &warmthSlider, &shineSlider, &driveSlider, &tightenSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, kParamBypass, bypassButton);

    setSize (780, 420);
}

void GRDWarmLiftAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void GRDWarmLiftAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDWarmLiftAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &warmthSlider, &shineSlider, &driveSlider, &tightenSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

void GRDWarmLiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDWarmLiftAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 4;
    warmthSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    shineSlider  .setBounds (top.removeFromLeft (topWidth).reduced (8));
    driveSlider  .setBounds (top.removeFromLeft (topWidth).reduced (8));
    tightenSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 3;
    mixSlider       .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDWarmLiftAudioProcessor();
}
