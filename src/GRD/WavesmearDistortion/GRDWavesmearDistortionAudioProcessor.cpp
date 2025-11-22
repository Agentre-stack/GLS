#include "GRDWavesmearDistortionAudioProcessor.h"

namespace
{
constexpr auto kStateId        = "WAVESMEAR_DISTORTION";
constexpr auto paramPreFilter  = "pre_filter";
constexpr auto paramSmear      = "smear_amount";
constexpr auto paramDrive      = "drive";
constexpr auto paramTone       = "tone";
constexpr auto paramMix        = "mix";
constexpr auto paramInputTrim  = "input_trim";
constexpr auto paramOutputTrim = "output_trim";
constexpr auto paramBypass     = "ui_bypass";
} // namespace

const std::array<GRDWavesmearDistortionAudioProcessor::Preset, 3>
    GRDWavesmearDistortionAudioProcessor::presetBank {{
        { "Smear Lead", {
            { paramPreFilter, 220.0f },
            { paramSmear,     0.35f },
            { paramDrive,     0.55f },
            { paramTone,      7600.0f },
            { paramMix,       0.65f },
            { paramInputTrim, 0.0f },
            { paramOutputTrim,-1.0f },
            { paramBypass,    0.0f }
        }},
        { "Drone Wash", {
            { paramPreFilter, 120.0f },
            { paramSmear,     0.7f },
            { paramDrive,     0.45f },
            { paramTone,      5200.0f },
            { paramMix,       0.55f },
            { paramInputTrim, -1.0f },
            { paramOutputTrim,-2.0f },
            { paramBypass,    0.0f }
        }},
        { "Bass Sputter", {
            { paramPreFilter, 80.0f },
            { paramSmear,     0.25f },
            { paramDrive,     0.75f },
            { paramTone,      3600.0f },
            { paramMix,       0.5f },
            { paramInputTrim, 1.0f },
            { paramOutputTrim,-3.0f },
            { paramBypass,    0.0f }
        }}
    }};

GRDWavesmearDistortionAudioProcessor::GRDWavesmearDistortionAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDWavesmearDistortionAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    preFilters.clear();
    toneFilters.clear();
    smearMemory.clear();
    dryBuffer.setSize (getTotalNumOutputChannels(), 0);
    lastBlockSize = 0;
}

void GRDWavesmearDistortionAudioProcessor::releaseResources()
{
}

void GRDWavesmearDistortionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float preFreq   = juce::jlimit (60.0f, 5000.0f, get (paramPreFilter));
    const float smear     = juce::jlimit (0.0f, 1.0f,   get (paramSmear));
    const float drive     = juce::jlimit (0.0f, 1.0f,   get (paramDrive));
    const float tone      = juce::jlimit (800.0f, 12000.0f, get (paramTone));
    const float mix       = juce::jlimit (0.0f, 1.0f,   get (paramMix));
    const float inputGain = juce::Decibels::decibelsToGain (get (paramInputTrim));
    const float outputGain= juce::Decibels::decibelsToGain (get (paramOutputTrim));
    const bool bypassed   = get (paramBypass) > 0.5f;

    updateFilters (preFreq, tone);
    const float driveGain = juce::jmap (drive, 1.0f, 18.0f);

    buffer.applyGain (inputGain);
    dryBuffer.makeCopyOf (buffer, true);

    if (bypassed)
    {
        buffer.applyGain (outputGain);
        return;
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr  = buffer.getWritePointer (ch);
        auto* dryPtr    = dryBuffer.getReadPointer (ch);
        auto& preFilter = preFilters[(size_t) ch];
        auto& toneFilter = toneFilters[(size_t) ch];
        auto& smearState = smearMemory[(size_t) ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float drySample = dryPtr[sample];
            const float preSample = preFilter.processSample (drySample);

            const float smearSample = preSample * (1.0f - smear) + smearState * smear;
            smearState = smearSample;

            const float saturated = juce::dsp::FastMathApproximations::tanh (smearSample * driveGain);
            const float toned = toneFilter.processSample (saturated);

            writePtr[sample] = (toned * mix) + (drySample * (1.0f - mix));
        }
    }

    buffer.applyGain (outputGain);
}

void GRDWavesmearDistortionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void GRDWavesmearDistortionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDWavesmearDistortionAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramPreFilter, "Pre Filter",
        juce::NormalisableRange<float> (60.0f, 5000.0f, 1.0f, 0.5f), 300.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramSmear, "Smear",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.35f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramDrive, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.6f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTone, "Tone",
        juce::NormalisableRange<float> (800.0f, 12000.0f, 1.0f, 0.4f), 6400.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMix, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramInputTrim, "Input Trim",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramOutputTrim, "Output Trim",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        paramBypass, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

int GRDWavesmearDistortionAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDWavesmearDistortionAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDWavesmearDistortionAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDWavesmearDistortionAudioProcessor::applyPreset (int index)
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

void GRDWavesmearDistortionAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if ((int) preFilters.size() < numChannels)
    {
        preFilters.resize (numChannels);
        toneFilters.resize (numChannels);
        smearMemory.resize (numChannels);
        std::fill (smearMemory.begin(), smearMemory.end(), 0.0f);

        for (int i = 0; i < numChannels; ++i)
        {
            preFilters[(size_t) i].reset();
            toneFilters[(size_t) i].reset();
        }
    }

    if ((int) dryBuffer.getNumChannels() != numChannels || (int) lastBlockSize != numSamples)
    {
        dryBuffer.setSize (numChannels, numSamples, false, false, true);
        lastBlockSize = static_cast<juce::uint32> (numSamples);
    }
}

void GRDWavesmearDistortionAudioProcessor::updateFilters (float preFreq, float toneFreq)
{
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, preFreq, 0.707f);
    auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, toneFreq, 0.707f);

    for (auto& filter : preFilters)
        filter.coefficients = hpCoeffs;

    for (auto& filter : toneFilters)
        filter.coefficients = toneCoeffs;
}

juce::AudioProcessorEditor* GRDWavesmearDistortionAudioProcessor::createEditor()
{
    return new GRDWavesmearDistortionAudioProcessorEditor (*this);
}

//==============================================================================
GRDWavesmearDistortionAudioProcessorEditor::GRDWavesmearDistortionAudioProcessorEditor (GRDWavesmearDistortionAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.WavesmearDistortion", "Wavesmear Distortion")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    initSlider (smearSlider,     "Smear", true);
    initSlider (driveSlider,     "Drive", true);
    initSlider (toneSlider,      "Tone");
    initSlider (mixSlider,       "Mix", true);
    initSlider (preFilterSlider, "Pre Filter");
    initSlider (inputTrimSlider, "Input");
    initSlider (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramSmear, smearSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDrive, driveSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTone,  toneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMix,   mixSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramPreFilter, preFilterSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramInputTrim, inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOutputTrim, outputTrimSlider));
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramBypass, bypassButton);

    setSize (760, 420);
}

void GRDWavesmearDistortionAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (10.0f), 10.0f);
}

void GRDWavesmearDistortionAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto topRow = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottomRow = area;

    auto topWidth = topRow.getWidth() / 4;
    smearSlider.setBounds (topRow.removeFromLeft (topWidth).reduced (10));
    driveSlider.setBounds (topRow.removeFromLeft (topWidth).reduced (10));
    toneSlider .setBounds (topRow.removeFromLeft (topWidth).reduced (10));
    mixSlider  .setBounds (topRow.removeFromLeft (topWidth).reduced (10));

    auto bottomWidth = bottomRow.getWidth() / 3;
    preFilterSlider .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (10));
    inputTrimSlider .setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (10));
    outputTrimSlider.setBounds (bottomRow.removeFromLeft (bottomWidth).reduced (10));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

void GRDWavesmearDistortionAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label, bool macro)
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

void GRDWavesmearDistortionAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDWavesmearDistortionAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &smearSlider, &driveSlider, &toneSlider, &mixSlider,
        &preFilterSlider, &inputTrimSlider, &outputTrimSlider
    };

    for (size_t i = 0; i < sliders.size() && i < labels.size(); ++i)
    {
        if (auto* s = sliders[i])
            labels[i]->setBounds (s->getBounds().withHeight (18).translated (0, -20));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDWavesmearDistortionAudioProcessor();
}
