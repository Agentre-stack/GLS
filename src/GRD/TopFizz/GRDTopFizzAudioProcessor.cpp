#include "GRDTopFizzAudioProcessor.h"

namespace
{
constexpr auto paramFreqId          = "freq";
constexpr auto paramAmountId        = "amount";
constexpr auto paramOddEvenBlendId  = "odd_even_blend";
constexpr auto paramDeHarshId       = "deharsh";
constexpr auto paramMixId           = "mix";
constexpr auto paramInputId         = "input_trim";
constexpr auto paramOutputId        = "output_trim";
constexpr auto paramBypassId        = "ui_bypass";
constexpr auto kStateId             = "TOP_FIZZ";
}

const std::array<GRDTopFizzAudioProcessor::Preset, 3> GRDTopFizzAudioProcessor::presetBank {{
    { "Vocal Air Fizz", {
        { paramFreqId,         9000.0f },
        { paramAmountId,          0.45f },
        { paramOddEvenBlendId,    0.35f },
        { paramDeHarshId,         0.6f },
        { paramMixId,             0.7f },
        { paramInputId,           0.0f },
        { paramOutputId,         -0.5f },
        { paramBypassId,          0.0f }
    }},
    { "Bright Guitar", {
        { paramFreqId,         7000.0f },
        { paramAmountId,          0.55f },
        { paramOddEvenBlendId,    0.5f },
        { paramDeHarshId,         0.5f },
        { paramMixId,             0.65f },
        { paramInputId,           0.0f },
        { paramOutputId,         -0.5f },
        { paramBypassId,          0.0f }
    }},
    { "Master Sparkle", {
        { paramFreqId,        12000.0f },
        { paramAmountId,          0.3f },
        { paramOddEvenBlendId,    0.6f },
        { paramDeHarshId,         0.75f },
        { paramMixId,             0.4f },
        { paramInputId,          -0.5f },
        { paramOutputId,         -0.8f },
        { paramBypassId,          0.0f }
    }}
}};

GRDTopFizzAudioProcessor::GRDTopFizzAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, kStateId, createParameterLayout())
{
}

void GRDTopFizzAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    highBandFilters.clear();
    smoothingFilters.clear();
    dryBuffer.setSize (getTotalNumOutputChannels(), 0);
    lastBlockSize = 0;
}

void GRDTopFizzAudioProcessor::releaseResources()
{
}

void GRDTopFizzAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    dryBuffer.makeCopyOf (buffer, true);

    const auto bandFreq = juce::jlimit (2000.0f, 16000.0f,
                                        apvts.getRawParameterValue (paramFreqId)->load());
    const auto amount   = juce::jlimit (0.0f, 1.0f,
                                        apvts.getRawParameterValue (paramAmountId)->load());
    const auto blend    = juce::jlimit (0.0f, 1.0f,
                                        apvts.getRawParameterValue (paramOddEvenBlendId)->load());
    const auto deHarsh  = juce::jlimit (0.0f, 1.0f,
                                        apvts.getRawParameterValue (paramDeHarshId)->load());
    const auto mix      = juce::jlimit (0.0f, 1.0f,
                                        apvts.getRawParameterValue (paramMixId)->load());
    const auto inputGain  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (paramInputId)->load());
    const auto outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (paramOutputId)->load());
    const bool bypassed   = apvts.getRawParameterValue (paramBypassId)->load() > 0.5f;

    const auto smoothFreq = juce::jmap (deHarsh, 4000.0f, 18000.0f);
    updateFilters (bandFreq, smoothFreq);

    buffer.applyGain (inputGain);
    if (bypassed)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        auto* dryPtr   = dryBuffer.getReadPointer (ch);
        auto& hpFilter = highBandFilters[ch];
        auto& lpFilter = smoothingFilters[ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float drySample  = dryPtr[sample];
            const float highBand   = hpFilter.processSample (drySample);
            const float harmonics  = lpFilter.processSample (generateHarmonics (highBand, amount, blend));
            const float wetSample  = drySample + harmonics;
            writePtr[sample] = (wetSample * mix + drySample * (1.0f - mix)) * outputGain;
        }
    }
}

void GRDTopFizzAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDTopFizzAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDTopFizzAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramFreqId, "Freq",
        juce::NormalisableRange<float> (2000.0f, 16000.0f, 1.0f, 0.45f), 8000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramAmountId, "Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramOddEvenBlendId, "Odd/Even",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramDeHarshId, "DeHarsh",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMixId, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramInputId, "Input Trim",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramOutputId, "Output Trim",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        paramBypassId, "Soft Bypass", false));

    return { params.begin(), params.end() };
}

void GRDTopFizzAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if ((int) highBandFilters.size() < numChannels)
    {
        highBandFilters.resize (numChannels);
        smoothingFilters.resize (numChannels);
        for (int i = 0; i < numChannels; ++i)
        {
            highBandFilters[i].reset();
            smoothingFilters[i].reset();
        }
    }

    if ((int) dryBuffer.getNumChannels() != numChannels || (int) lastBlockSize != numSamples)
    {
        dryBuffer.setSize (numChannels, numSamples, false, false, true);
        lastBlockSize = static_cast<juce::uint32> (numSamples);
    }
}

void GRDTopFizzAudioProcessor::updateFilters (float bandFreq, float smoothFreq)
{
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, bandFreq, 0.707f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, smoothFreq, 0.707f);

    for (auto& filter : highBandFilters)
        filter.coefficients = hpCoeffs;

    for (auto& filter : smoothingFilters)
        filter.coefficients = lpCoeffs;
}

float GRDTopFizzAudioProcessor::generateHarmonics (float input, float amount, float blend) const
{
    const float drive  = juce::jmap (amount, 1.0f, 10.0f);
    const float driven = input * drive;

    const float oddComponent  = juce::dsp::FastMathApproximations::tanh (driven);
    const float evenComponent = juce::dsp::FastMathApproximations::tanh (driven + 0.35f * driven * driven);
    const float harmonic      = juce::jmap (blend, oddComponent, evenComponent);

    return harmonic * juce::jmap (amount, 0.0f, 1.0f, 0.0f, 1.5f);
}

juce::AudioProcessorEditor* GRDTopFizzAudioProcessor::createEditor()
{
    return new GRDTopFizzAudioProcessorEditor (*this);
}

GRDTopFizzAudioProcessorEditor::GRDTopFizzAudioProcessorEditor (GRDTopFizzAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      accentColour (gls::ui::accentForFamily ("GRD")),
      headerComponent ("GRD.TopFizz", "Top Fizz")
{
    lookAndFeel.setAccentColour (accentColour);
    setLookAndFeel (&lookAndFeel);
    headerComponent.setAccentColour (accentColour);
    footerComponent.setAccentColour (accentColour);

    addAndMakeVisible (headerComponent);
    addAndMakeVisible (footerComponent);

    initSlider (freqSlider,      "Freq", true);
    initSlider (amountSlider,    "Amount", true);
    initSlider (oddEvenSlider,   "Odd/Even");
    initSlider (deHarshSlider,   "DeHarsh");
    initSlider (mixSlider,       "Mix");
    initSlider (inputTrimSlider, "Input");
    initSlider (outputTrimSlider,"Output");
    initToggle (bypassButton);

    auto& state = processorRef.getValueTreeState();

    attachments.push_back (std::make_unique<SliderAttachment> (state, paramFreqId, freqSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramAmountId, amountSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOddEvenBlendId, oddEvenSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDeHarshId, deHarshSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMixId, mixSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramInputId, inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOutputId, outputTrimSlider));
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramBypassId, bypassButton));

    setSize (760, 420);
}

void GRDTopFizzAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (gls::ui::Colours::background());
    auto body = getLocalBounds().withTrimmedTop (64).withTrimmedBottom (64);
    g.setColour (gls::ui::Colours::panel().darker (0.2f));
    g.fillRoundedRectangle (body.toFloat().reduced (8.0f), 10.0f);
}

void GRDTopFizzAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    headerComponent.setBounds (bounds.removeFromTop (64));
    footerComponent.setBounds (bounds.removeFromBottom (64));

    auto area = bounds.reduced (12);
    auto top = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.55f));
    auto bottom = area;

    auto topWidth = top.getWidth() / 4;
    freqSlider   .setBounds (top.removeFromLeft (topWidth).reduced (8));
    amountSlider .setBounds (top.removeFromLeft (topWidth).reduced (8));
    oddEvenSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));
    deHarshSlider.setBounds (top.removeFromLeft (topWidth).reduced (8));

    auto bottomWidth = bottom.getWidth() / 3;
    mixSlider       .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    inputTrimSlider .setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));
    outputTrimSlider.setBounds (bottom.removeFromLeft (bottomWidth).reduced (8));

    bypassButton.setBounds (footerComponent.getBounds().reduced (24, 12));
    layoutLabels();
}

void GRDTopFizzAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name, bool macro)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, macro ? 72 : 64, 18);
    slider.setName (name);
    addAndMakeVisible (slider);

    auto lab = std::make_unique<juce::Label>();
    lab->setText (name, juce::dontSendNotification);
    lab->setJustificationType (juce::Justification::centred);
    lab->setColour (juce::Label::textColourId, gls::ui::Colours::text());
    lab->setFont (gls::ui::makeFont (12.0f));
    addAndMakeVisible (*lab);
    labels.push_back (std::move (lab));
}

void GRDTopFizzAudioProcessorEditor::initToggle (juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel (&lookAndFeel);
    toggle.setClickingTogglesState (true);
    addAndMakeVisible (toggle);
}

void GRDTopFizzAudioProcessorEditor::layoutLabels()
{
    std::vector<juce::Slider*> sliders {
        &freqSlider, &amountSlider, &oddEvenSlider, &deHarshSlider,
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
    return new GRDTopFizzAudioProcessor();
}

int GRDTopFizzAudioProcessor::getNumPrograms()
{
    return (int) presetBank.size();
}

const juce::String GRDTopFizzAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presetBank.size()))
        return presetBank[(size_t) index].name;
    return {};
}

void GRDTopFizzAudioProcessor::setCurrentProgram (int index)
{
    const int clamped = juce::jlimit (0, (int) presetBank.size() - 1, index);
    if (clamped == currentPreset)
        return;

    currentPreset = clamped;
    applyPreset (clamped);
}

void GRDTopFizzAudioProcessor::applyPreset (int index)
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
