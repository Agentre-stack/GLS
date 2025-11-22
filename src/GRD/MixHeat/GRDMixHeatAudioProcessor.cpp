#include "GRDMixHeatAudioProcessor.h"

namespace
{
constexpr auto kStateId    = "MIX_HEAT";
constexpr auto paramMode   = "mode";
constexpr auto paramDrive  = "drive";
constexpr auto paramTone   = "tone";
constexpr auto paramMix    = "mix";
constexpr auto paramInput  = "input_trim";
constexpr auto paramOutput = "output_trim";
constexpr auto paramBypass = "ui_bypass";
}

const std::array<GRDMixHeatAudioProcessor::Preset, 3> GRDMixHeatAudioProcessor::presetBank {{
    { "Clean Glue", {
        { paramMode,    0.0f },
        { paramDrive,   0.35f },
        { paramTone,   -0.1f },
        { paramMix,     0.6f },
        { paramInput,   0.0f },
        { paramOutput, -0.5f },
        { paramBypass,  0.0f }
    }},
    { "Tape Heat", {
        { paramMode,    1.0f },
        { paramDrive,   0.55f },
        { paramTone,    0.15f },
        { paramMix,     0.65f },
        { paramInput,   0.0f },
        { paramOutput, -1.0f },
        { paramBypass,  0.0f }
    }},
    { "Triode Push", {
        { paramMode,    2.0f },
        { paramDrive,   0.7f },
        { paramTone,    0.25f },
        { paramMix,     0.7f },
        { paramInput,  -0.5f },
        { paramOutput, -1.5f },
        { paramBypass,  0.0f }
    }}
}};

GRDMixHeatAudioProcessor::GRDMixHeatAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDMixHeatAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    toneFilter.reset();
}

void GRDMixHeatAudioProcessor::releaseResources()
{
}

void GRDMixHeatAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int mode      = (int) std::round (apvts.getRawParameterValue (paramMode)->load());
    const float drive   = juce::jlimit (0.0f, 1.0f, get (paramDrive));
    const float tone    = juce::jlimit (-1.0f, 1.0f, get (paramTone));
    const float mix     = juce::jlimit (0.0f, 1.0f, get (paramMix));
    const float inputGain  = juce::Decibels::decibelsToGain (get (paramInput));
    const float outputGain = juce::Decibels::decibelsToGain (get (paramOutput));
    const bool bypassed    = apvts.getRawParameterValue (paramBypass)->load() > 0.5f;

    const float toneFreq = juce::jmap (tone, -1.0f, 1.0f, 800.0f, 8000.0f);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, toneFreq, 0.8f);
    toneFilter.coefficients = coeffs;

    const float driveGain = driveToGain (drive);

    buffer.applyGain (inputGain);
    if (bypassed)
    {
        buffer.applyGain (outputGain);
        return;
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float dry = data[i];
            float wet = applySaturation (dry * driveGain, drive, mode);
            wet = toneFilter.processSample (wet);
            data[i] = (wet * mix + dry * (1.0f - mix)) * outputGain;
        }
    }
}

void GRDMixHeatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void GRDMixHeatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorEditor* GRDMixHeatAudioProcessor::createEditor()
{
    return new GRDMixHeatAudioProcessorEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDMixHeatAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice>(paramMode, "Mode",
                                                                   juce::StringArray { "Clean", "Tape", "Triode" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramDrive,  "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramTone,   "Tone",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramMix,    "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramInput,  "Input Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (paramOutput, "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (paramBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

int GRDMixHeatAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDMixHeatAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDMixHeatAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDMixHeatAudioProcessor::applyPreset (int index)
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

//==============================================================================
GRDMixHeatAudioProcessorEditor::GRDMixHeatAudioProcessorEditor (GRDMixHeatAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.MixHeat", "Mix Heat")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    initSlider (driveSlider,  "Drive", true);
    initSlider (toneSlider,   "Tone");
    initSlider (mixSlider,    "Mix");
    initSlider (inputTrimSlider, "Input");
    initSlider (outputTrimSlider,"Output");
    initToggle (bypassButton);

    modeBox.addItemList ({ "Clean", "Tape", "Triode" }, 1);
    modeBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modeBox);

    auto& state = processorRef.getValueTreeState();
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramDrive,  driveSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramTone,   toneSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramMix,    mixSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramInput,  inputTrimSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, paramOutput, outputTrimSlider));
    modeAttachment = std::make_unique<ComboAttachment> (state, paramMode, modeBox);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramBypass, bypassButton);

    setSize (720, 400);
}

void GRDMixHeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDMixHeatAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    modeBox.setBounds (area.removeFromTop (36).reduced (8));

    auto row = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto rowWidth = row.getWidth() / 3;
    driveSlider .setBounds (row.removeFromLeft (rowWidth).reduced (8));
    toneSlider  .setBounds (row.removeFromLeft (rowWidth).reduced (8));
    mixSlider   .setBounds (row.removeFromLeft (rowWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 2;
    inputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
}

void GRDMixHeatAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void GRDMixHeatAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDMixHeatAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders { &driveSlider, &toneSlider, &mixSlider, &inputTrimSlider, &outputTrimSlider };
    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDMixHeatAudioProcessor();
}

float GRDMixHeatAudioProcessor::driveToGain (float drive) const
{
    return juce::jmap (drive, 0.0f, 1.0f, 1.0f, 10.0f);
}

float GRDMixHeatAudioProcessor::applySaturation (float sample, float drive, int mode) const
{
    const float clean = juce::dsp::FastMathApproximations::tanh (sample);

    if (mode == 0) return clean;

    if (mode == 1)
    {
        const float tape = sample - (sample * sample * sample) * 0.3f;
        return juce::jmap (drive, 0.0f, 1.0f, clean, tape);
    }

    const float triode = juce::dsp::FastMathApproximations::tanh (sample * 1.5f + sample * sample * sample * 0.2f);
    return juce::jmap (drive, 0.0f, 1.0f, clean, triode);
}
