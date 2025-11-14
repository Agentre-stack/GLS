#include "GRDTopFizzAudioProcessor.h"

namespace
{
constexpr auto paramFreqId          = "freq";
constexpr auto paramAmountId        = "amount";
constexpr auto paramOddEvenBlendId  = "odd_even_blend";
constexpr auto paramDeHarshId       = "deharsh";
constexpr auto paramMixId           = "mix";
}

GRDTopFizzAudioProcessor::GRDTopFizzAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TOP_FIZZ", createParameterLayout())
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

    const auto smoothFreq = juce::jmap (deHarsh, 4000.0f, 18000.0f);
    updateFilters (bandFreq, smoothFreq);

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
            writePtr[sample] = wetSample * mix + drySample * (1.0f - mix);
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
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (freqSlider,      "Freq");
    initSlider (amountSlider,    "Amount");
    initSlider (oddEvenSlider,   "Odd / Even");
    initSlider (deHarshSlider,   "DeHarsh");
    initSlider (mixSlider,       "Mix");

    auto& state = processorRef.getValueTreeState();

    attachments.push_back (std::make_unique<SliderAttachment> (state, paramFreqId, freqSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramAmountId, amountSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOddEvenBlendId, oddEvenSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDeHarshId, deHarshSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMixId, mixSlider));

    setSize (600, 260);
}

void GRDTopFizzAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("GRD Top Fizz", getLocalBounds().removeFromTop (28), juce::Justification::centred, 1);
}

void GRDTopFizzAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (12);
    bounds.removeFromTop (30);

    auto row = bounds.removeFromTop (bounds.getHeight());
    auto columnWidth = row.getWidth() / 5;
    freqSlider   .setBounds (row.removeFromLeft (columnWidth).reduced (8));
    amountSlider .setBounds (row.removeFromLeft (columnWidth).reduced (8));
    oddEvenSlider.setBounds (row.removeFromLeft (columnWidth).reduced (8));
    deHarshSlider.setBounds (row.removeFromLeft (columnWidth).reduced (8));
    mixSlider    .setBounds (row.removeFromLeft (columnWidth).reduced (8));
}

void GRDTopFizzAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}
