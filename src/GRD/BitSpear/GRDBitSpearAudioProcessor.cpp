#include "GRDBitSpearAudioProcessor.h"

namespace
{
constexpr auto kStateId     = "BIT_SPEAR";
constexpr auto kParamBypass = "ui_bypass";
constexpr auto kParamInput  = "input_trim";
constexpr auto kParamOutput = "output_trim";
}

const std::array<GRDBitSpearAudioProcessor::Preset, 3> GRDBitSpearAudioProcessor::presetBank {{
    { "Lo-Fi Vox", {
        { "bits",        10.0f },
        { "downsample",   4.0f },
        { "drive",        0.45f },
        { "mix",          0.6f },
        { kParamInput,    0.0f },
        { kParamOutput,  -1.0f },
        { kParamBypass,   0.0f }
    }},
    { "Bit Snare", {
        { "bits",         8.0f },
        { "downsample",   3.0f },
        { "drive",        0.65f },
        { "mix",          0.8f },
        { kParamInput,    0.0f },
        { kParamOutput,   0.0f },
        { kParamBypass,   0.0f }
    }},
    { "8-Bit Lead", {
        { "bits",         6.0f },
        { "downsample",   6.0f },
        { "drive",        0.55f },
        { "mix",          0.9f },
        { kParamInput,   -1.0f },
        { kParamOutput,  -1.5f },
        { kParamBypass,   0.0f }
    }}
}};

GRDBitSpearAudioProcessor::GRDBitSpearAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDBitSpearAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
}

void GRDBitSpearAudioProcessor::releaseResources()
{
}

void GRDBitSpearAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const float bitsParam     = juce::jlimit (4.0f, 16.0f, get ("bits"));
    const int downsampleSteps = juce::jlimit (1, 16, (int) std::round (get ("downsample")));
    const float drive         = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float mix           = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float inputGain     = juce::Decibels::decibelsToGain (juce::jlimit (-18.0f, 18.0f, get (kParamInput)));
    const float trimGain      = juce::Decibels::decibelsToGain (juce::jlimit (-18.0f, 18.0f, get (kParamOutput)));
    const bool bypassed       = get (kParamBypass) > 0.5f;

    ensureStateSize (numChannels);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    const float maxCode = std::pow (2.0f, bitsParam - 1.0f) - 1.0f;
    const float invStep = maxCode > 0.0f ? 1.0f / maxCode : 1.0f;
    const float crushScale = 1.0f + drive * 7.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = data[i];

            if (state.downsampleCounter++ >= downsampleSteps)
            {
                state.downsampleCounter = 0;
                state.heldSample = input;
            }

            const float crushed = std::round (juce::jlimit (-1.0f, 1.0f, state.heldSample) * maxCode) * invStep;
            const float driven = juce::jlimit (-1.0f, 1.0f, std::tanh (crushed * crushScale));
            const float blended = juce::jmap (mix, dryBuffer.getReadPointer (ch)[i], driven);
            data[i] = blended * trimGain;
        }
    }
}

void GRDBitSpearAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDBitSpearAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDBitSpearAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bits", "Bits",
                                                                   juce::NormalisableRange<float> (4.0f, 16.0f, 0.01f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("downsample", "Downsample",
                                                                   juce::NormalisableRange<float> (1.0f, 16.0f, 1.0f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (kParamInput,  "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (kParamBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GRDBitSpearAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
    {
        channelState.clear();
        dryBuffer.setSize (0, 0);
        return;
    }

    if ((int) channelState.size() < numChannels)
        channelState.resize ((size_t) numChannels);
}

int GRDBitSpearAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDBitSpearAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDBitSpearAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDBitSpearAudioProcessor::applyPreset (int index)
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

GRDBitSpearAudioProcessorEditor::GRDBitSpearAudioProcessorEditor (GRDBitSpearAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.BitSpear", "Bit Spear")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    auto make = [this](juce::Slider& slider, const juce::String& label, bool macro = false)
    {
        initSlider (slider, label, macro);
    };

    make (bitsSlider,       "Bits", true);
    make (downsampleSlider, "Downsample", true);
    make (driveSlider,      "Drive");
    make (mixSlider,        "Mix");
    make (inputTrimSlider,  "Input");
    make (outputTrimSlider, "Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "bits", "downsample", "drive", "mix", kParamInput, kParamOutput };
    juce::Slider* sliders[] = { &bitsSlider, &downsampleSlider, &driveSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, kParamBypass, bypassButton));

    setSize (720, 420);
}

void GRDBitSpearAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void GRDBitSpearAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDBitSpearAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &bitsSlider, &downsampleSlider, &driveSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

void GRDBitSpearAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDBitSpearAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    const int topWidth = top.getWidth() / 3;
    bitsSlider      .setBounds (top.removeFromLeft (topWidth).reduced (8));
    downsampleSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));
    driveSlider     .setBounds (top.removeFromLeft (topWidth).reduced (8));

    const int bottomWidth = bottom.getWidth() / 3;
    mixSlider       .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

juce::AudioProcessorEditor* GRDBitSpearAudioProcessor::createEditor()
{
    return new GRDBitSpearAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDBitSpearAudioProcessor();
}
