#include "GRDFaultLineFuzzAudioProcessor.h"

namespace
{
constexpr auto paramInputTrim = "input_trim";
constexpr auto paramFuzz      = "fuzz";
constexpr auto paramBias      = "bias";
constexpr auto paramGate      = "gate";
constexpr auto paramTone      = "tone";
constexpr auto paramOutput    = "output_trim";
}

GRDFaultLineFuzzAudioProcessor::GRDFaultLineFuzzAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FAULT_LINE_FUZZ", createParameterLayout())
{
}

void GRDFaultLineFuzzAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (44100.0, sampleRate);
    processingBuffer.setSize (getTotalNumOutputChannels(), 0);
    toneFilters.clear();
    gateState.assign ((size_t) getTotalNumOutputChannels(), 0.0f);
}

void GRDFaultLineFuzzAudioProcessor::releaseResources()
{
}

void GRDFaultLineFuzzAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    processingBuffer.makeCopyOf (buffer, true);

    const auto inputDb  = apvts.getRawParameterValue (paramInputTrim)->load();
    const auto fuzz     = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramFuzz)->load());
    const auto bias     = juce::jlimit (-1.0f, 1.0f, apvts.getRawParameterValue (paramBias)->load());
    const auto gate     = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramGate)->load());
    const auto toneHz   = apvts.getRawParameterValue (paramTone)->load();
    const auto outputDb = apvts.getRawParameterValue (paramOutput)->load();

    const auto inGain  = juce::Decibels::decibelsToGain (inputDb);
    const auto outGain = juce::Decibels::decibelsToGain (outputDb);
    const float gateThreshold = juce::jmap (gate, 0.02f, 0.3f);
    const float gateRelease   = juce::jmap (gate, 0.1f, 0.6f);

    auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                        juce::jlimit (400.0f, 12000.0f, toneHz),
                                                                        0.707f);
    for (auto& filter : toneFilters)
        filter.coefficients = toneCoeffs;

    const float fuzzDrive = juce::jmap (fuzz, 2.0f, 40.0f);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        auto* procPtr  = processingBuffer.getWritePointer (ch);
        float& gateEnv = gateState[(size_t) ch];
        auto& toneFilter = toneFilters[(size_t) ch];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float x = procPtr[sample] * inGain;
            const float biased = x + bias * 0.5f;
            const float fuzzed = juce::dsp::FastMathApproximations::tanh (biased * fuzzDrive);

            const float level = std::abs (fuzzed);
            gateEnv = level > gateEnv ? level : gateEnv * gateRelease + level * (1.0f - gateRelease);
            const float gateGain = (gateEnv < gateThreshold) ? juce::jmap (gateEnv / gateThreshold, 0.0f, 1.0f, 0.0f, 1.0f)
                                                             : 1.0f;

            float toned = toneFilter.processSample (fuzzed * gateGain);
            writePtr[sample] = toned * outGain;
        }
    }
}

void GRDFaultLineFuzzAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDFaultLineFuzzAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDFaultLineFuzzAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramInputTrim, "Input Trim",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramFuzz, "Fuzz",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramBias, "Bias",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramGate, "Gate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramTone, "Tone",
        juce::NormalisableRange<float> (400.0f, 12000.0f, 1.0f, 0.45f), 4500.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramOutput, "Output Trim",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

void GRDFaultLineFuzzAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if ((int) processingBuffer.getNumChannels() != numChannels
        || processingBuffer.getNumSamples() != numSamples)
        processingBuffer.setSize (numChannels, numSamples, false, false, true);

    if ((int) gateState.size() < numChannels)
        gateState.resize ((size_t) numChannels, 0.0f);

    if ((int) toneFilters.size() < numChannels)
    {
        toneFilters.resize (numChannels);
        for (auto& filter : toneFilters)
            filter.reset();
    }
}

juce::AudioProcessorEditor* GRDFaultLineFuzzAudioProcessor::createEditor()
{
    return new GRDFaultLineFuzzAudioProcessorEditor (*this);
}

//==============================================================================
GRDFaultLineFuzzAudioProcessorEditor::GRDFaultLineFuzzAudioProcessorEditor (GRDFaultLineFuzzAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (inputTrimSlider, "Input");
    initSlider (fuzzSlider,      "Fuzz");
    initSlider (biasSlider,      "Bias");
    initSlider (gateSlider,      "Gate");
    initSlider (toneSlider,      "Tone");
    initSlider (outputTrimSlider,"Output");

    auto& state = processorRef.getValueTreeState();
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramInputTrim, inputTrimSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramFuzz, fuzzSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramBias, biasSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramGate, gateSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramTone, toneSlider));
    attachments.push_back (std::make_unique<SliderAttachment> (state, paramOutput, outputTrimSlider));

    setSize (640, 260);
}

void GRDFaultLineFuzzAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("GRD Fault Line Fuzz", getLocalBounds().removeFromTop (28),
                      juce::Justification::centred, 1);
}

void GRDFaultLineFuzzAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (30);
    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 6;
    inputTrimSlider .setBounds (row.removeFromLeft (width).reduced (6));
    fuzzSlider      .setBounds (row.removeFromLeft (width).reduced (6));
    biasSlider      .setBounds (row.removeFromLeft (width).reduced (6));
    gateSlider      .setBounds (row.removeFromLeft (width).reduced (6));
    toneSlider      .setBounds (row.removeFromLeft (width).reduced (6));
    outputTrimSlider.setBounds (row.removeFromLeft (width).reduced (6));
}

void GRDFaultLineFuzzAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}
