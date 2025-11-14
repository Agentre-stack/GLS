#include "GRDTransTubeXAudioProcessor.h"

namespace
{
constexpr auto paramDrive      = "drive";
constexpr auto paramTransSens  = "trans_sens";
constexpr auto paramAttackBias = "attack_bias";
constexpr auto paramTone       = "tone";
constexpr auto paramMix        = "mix";
}

GRDTransTubeXAudioProcessor::GRDTransTubeXAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRANS_TUBE_X", createParameterLayout())
{
}

void GRDTransTubeXAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (44100.0, sampleRate);
    trackers.clear();
    toneFilters.clear();
    dryBuffer.setSize (getTotalNumOutputChannels(), 0);
}

void GRDTransTubeXAudioProcessor::releaseResources()
{
}

void GRDTransTubeXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    dryBuffer.makeCopyOf (buffer, true);

    const auto drive     = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramDrive)->load());
    const auto sens      = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramTransSens)->load());
    const auto attack    = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramAttackBias)->load());
    const auto toneHz    = juce::jlimit (500.0f, 12000.0f, apvts.getRawParameterValue (paramTone)->load());
    const auto mix       = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramMix)->load());

    const float driveGain = juce::jmap (drive, 1.0f, 18.0f);
    const float transientScale = juce::jmap (sens, 0.0f, 1.0f, 0.0f, 4.0f);
    const float attackBlend = juce::jmap (attack, 0.0f, 1.0f, 0.2f, 0.95f);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, toneHz, 0.707f);
    for (auto& filter : toneFilters)
        filter.coefficients = coeffs;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        auto* dryPtr   = dryBuffer.getReadPointer (ch);
        auto& tracker  = trackers[(size_t) ch];
        auto& toneFilter = toneFilters[(size_t) ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float drySample = dryPtr[sample];
            const float transient = tracker.process (drySample) * transientScale;

            const float driveMod = 1.0f + transient * attackBlend;
            const float attacked = drySample * (1.0f + transient * (1.0f - attackBlend));
            const float tubeIn   = attacked * driveGain * driveMod;

            float shaped = juce::dsp::FastMathApproximations::tanh (tubeIn);
            shaped = toneFilter.processSample (shaped);

            writePtr[sample] = shaped * mix + drySample * (1.0f - mix);
        }
    }
}

void GRDTransTubeXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDTransTubeXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDTransTubeXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramDrive, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.6f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTransSens, "Trans Sens",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramAttackBias, "Attack Bias",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTone, "Tone",
        juce::NormalisableRange<float> (500.0f, 12000.0f, 1.0f, 0.4f), 6000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMix, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    return { params.begin(), params.end() };
}

void GRDTransTubeXAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if ((int) trackers.size() < numChannels)
    {
        trackers.resize (numChannels);
        for (auto& tracker : trackers)
        {
            tracker.setSampleRate (currentSampleRate);
            tracker.setTimes (2.0f, 40.0f);
            tracker.reset();
        }
    }

    if ((int) toneFilters.size() < numChannels)
    {
        toneFilters.resize (numChannels);
        for (auto& filter : toneFilters)
            filter.reset();
    }

    if ((int) dryBuffer.getNumChannels() != numChannels
        || dryBuffer.getNumSamples() != numSamples)
        dryBuffer.setSize (numChannels, numSamples, false, false, true);
}

juce::AudioProcessorEditor* GRDTransTubeXAudioProcessor::createEditor()
{
    return new GRDTransTubeXAudioProcessorEditor (*this);
}

//==============================================================================
GRDTransTubeXAudioProcessorEditor::GRDTransTubeXAudioProcessorEditor (GRDTransTubeXAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (driveSlider,      "Drive");
    initSlider (transSensSlider,  "Trans Sens");
    initSlider (attackBiasSlider, "Attack Bias");
    initSlider (toneSlider,       "Tone");
    initSlider (mixSlider,        "Mix");

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramDrive, driveSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTransSens, transSensSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramAttackBias, attackBiasSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTone, toneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramMix, mixSlider));

    setSize (620, 240);
}

void GRDTransTubeXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("GRD Trans Tube X", getLocalBounds().removeFromTop (28),
                      juce::Justification::centred, 1);
}

void GRDTransTubeXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (30);
    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 5;
    driveSlider     .setBounds (row.removeFromLeft (width).reduced (6));
    transSensSlider .setBounds (row.removeFromLeft (width).reduced (6));
    attackBiasSlider.setBounds (row.removeFromLeft (width).reduced (6));
    toneSlider      .setBounds (row.removeFromLeft (width).reduced (6));
    mixSlider       .setBounds (row.removeFromLeft (width).reduced (6));
}

void GRDTransTubeXAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}
