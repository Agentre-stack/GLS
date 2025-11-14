#include "GRDWavesmearDistortionAudioProcessor.h"

namespace
{
constexpr auto paramPreFilterId  = "pre_filter";
constexpr auto paramSmearId      = "smear_amount";
constexpr auto paramDriveId      = "drive";
constexpr auto paramToneId       = "tone";
constexpr auto paramMixId        = "mix";
}

GRDWavesmearDistortionAudioProcessor::GRDWavesmearDistortionAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WAVESMEAR_DISTORTION", createParameterLayout())
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

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    dryBuffer.makeCopyOf (buffer, true);

    const auto preFreq = juce::jlimit (60.0f, 5000.0f,
                                       apvts.getRawParameterValue (paramPreFilterId)->load());
    const auto smear   = juce::jlimit (0.0f, 1.0f,
                                       apvts.getRawParameterValue (paramSmearId)->load());
    const auto drive   = juce::jlimit (0.0f, 1.0f,
                                       apvts.getRawParameterValue (paramDriveId)->load());
    const auto tone    = juce::jlimit (800.0f, 12000.0f,
                                       apvts.getRawParameterValue (paramToneId)->load());
    const auto mix     = juce::jlimit (0.0f, 1.0f,
                                       apvts.getRawParameterValue (paramMixId)->load());

    updateFilters (preFreq, tone);
    const float driveGain = juce::jmap (drive, 1.0f, 18.0f);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr  = buffer.getWritePointer (ch);
        auto* dryPtr    = dryBuffer.getReadPointer (ch);
        auto& preFilter = preFilters[ch];
        auto& toneFilter = toneFilters[ch];
        auto& smearState = smearMemory[ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float drySample = dryPtr[sample];
            const float preSample = preFilter.processSample (drySample);

            const float smearSample = preSample * (1.0f - smear) + smearState * smear;
            smearState = smearSample;

            const float saturated = juce::dsp::FastMathApproximations::tanh (smearSample * driveGain);
            const float toned = toneFilter.processSample (saturated);

            writePtr[sample] = toned * mix + drySample * (1.0f - mix);
        }
    }
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
        paramPreFilterId, "Pre Filter",
        juce::NormalisableRange<float> (60.0f, 5000.0f, 1.0f, 0.5f), 400.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramSmearId, "Smear",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.4f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramDriveId, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.6f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramToneId, "Tone",
        juce::NormalisableRange<float> (800.0f, 12000.0f, 1.0f, 0.4f), 6000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMixId, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    return { params.begin(), params.end() };
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
            preFilters[i].reset();
            toneFilters[i].reset();
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

GRDWavesmearDistortionAudioProcessorEditor::GRDWavesmearDistortionAudioProcessorEditor (GRDWavesmearDistortionAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (preFilterSlider, "Pre Filter");
    initSlider (smearSlider,     "Smear");
    initSlider (driveSlider,     "Drive");
    initSlider (toneSlider,      "Tone");
    initSlider (mixSlider,       "Mix");

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramPreFilterId, preFilterSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramSmearId, smearSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDriveId, driveSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramToneId, toneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMixId, mixSlider));

    setSize (600, 260);
}

void GRDWavesmearDistortionAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("GRD Wavesmear Distortion", getLocalBounds().removeFromTop (28),
                      juce::Justification::centred, 1);
}

void GRDWavesmearDistortionAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (12);
    bounds.removeFromTop (30);
    auto row = bounds.removeFromTop (bounds.getHeight());
    auto columnWidth = row.getWidth() / 5;

    preFilterSlider.setBounds (row.removeFromLeft (columnWidth).reduced (8));
    smearSlider    .setBounds (row.removeFromLeft (columnWidth).reduced (8));
    driveSlider    .setBounds (row.removeFromLeft (columnWidth).reduced (8));
    toneSlider     .setBounds (row.removeFromLeft (columnWidth).reduced (8));
    mixSlider      .setBounds (row.removeFromLeft (columnWidth).reduced (8));
}

void GRDWavesmearDistortionAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}
