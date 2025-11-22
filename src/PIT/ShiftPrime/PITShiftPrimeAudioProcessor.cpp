#include "PITShiftPrimeAudioProcessor.h"

namespace
{
constexpr auto paramSemitones = "semitones";
constexpr auto paramCents     = "cents";
constexpr auto paramFormant   = "formant";
constexpr auto paramHPF       = "hpf";
constexpr auto paramLPF       = "lpf";
constexpr auto paramMode      = "mode";
constexpr auto paramMix       = "mix";
}

PITShiftPrimeAudioProcessor::PITShiftPrimeAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PIT_SHIFT_PRIME", createParameterLayout())
{
}

void PITShiftPrimeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto totalChannels = juce::jmax (2, getTotalNumOutputChannels());
    const auto blockSize = juce::jmax (1, samplesPerBlock);

    hpfFilters.clear();
    lpfFilters.clear();
    formantFilters.clear();
    dryBuffer.setSize (totalChannels, blockSize);
    wetBuffer.setSize (totalChannels, blockSize);

    pitchShifter.prepare (currentSampleRate, totalChannels);
    pitchShifter.reset();
}

void PITShiftPrimeAudioProcessor::releaseResources()
{
}

void PITShiftPrimeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (auto ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels(), buffer.getNumSamples());
    dryBuffer.makeCopyOf (buffer, true);
    wetBuffer.makeCopyOf (buffer, true);

    const auto semitones = apvts.getRawParameterValue (paramSemitones)->load();
    const auto cents     = apvts.getRawParameterValue (paramCents)->load();
    const auto formant   = apvts.getRawParameterValue (paramFormant)->load();
    const auto hpfFreq   = apvts.getRawParameterValue (paramHPF)->load();
    const auto lpfFreq   = apvts.getRawParameterValue (paramLPF)->load();
    const auto mode      = (int) std::round (apvts.getRawParameterValue (paramMode)->load());
    const auto mix       = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (paramMix)->load());

    updateFilters (hpfFreq, lpfFreq, formant);

    const float ratio = std::pow (2.0f, (semitones + cents / 100.0f) / 12.0f);
    const float drive = mode == 1 ? juce::jmap (std::abs (semitones), 0.0f, 12.0f, 1.0f, 2.5f) : 1.0f;

    pitchShifter.process (wetBuffer, ratio);

    for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
    {
        auto* data = wetBuffer.getWritePointer (ch);
        auto& hpf  = hpfFilters[(size_t) ch];
        auto& lpf  = lpfFilters[(size_t) ch];
        auto& form = formantFilters[(size_t) ch];

        for (int i = 0; i < wetBuffer.getNumSamples(); ++i)
        {
            float sample = data[i];
            sample = hpf.processSample (sample);
            sample = lpf.processSample (sample);
            sample = form.processSample (sample);

            if (mode == 1)
                sample = juce::dsp::FastMathApproximations::tanh (sample * drive);

            data[i] = sample;
        }
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dry = dryBuffer.getReadPointer (ch);
        auto* wet = wetBuffer.getReadPointer (ch);
        auto* out = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            out[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }
}

void PITShiftPrimeAudioProcessor::ensureStateSize (int numChannels, int numSamples)
{
    if (dryBuffer.getNumChannels() != numChannels || dryBuffer.getNumSamples() != numSamples)
        dryBuffer.setSize (numChannels, numSamples, false, false, true);

    if (wetBuffer.getNumChannels() != numChannels || wetBuffer.getNumSamples() != numSamples)
        wetBuffer.setSize (numChannels, numSamples, false, false, true);

    auto ensureFilter = [numChannels](auto& filters)
    {
        if ((int) filters.size() < numChannels)
        {
            filters.resize (numChannels);
            for (auto& filter : filters)
                filter.reset();
        }
    };

    ensureFilter (hpfFilters);
    ensureFilter (lpfFilters);
    ensureFilter (formantFilters);
}

void PITShiftPrimeAudioProcessor::updateFilters (float hpf, float lpf, float formant)
{
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                        juce::jlimit (20.0f, 2000.0f, hpf),
                                                                        0.707f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                      juce::jlimit (500.0f, 20000.0f, lpf),
                                                                      0.707f);
    auto formantCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate,
                                                                              juce::jmap (formant, 500.0f, 5000.0f),
                                                                              1.0f,
                                                                              1.5f);

    for (auto& f : hpfFilters)
        f.coefficients = hpCoeffs;
    for (auto& f : lpfFilters)
        f.coefficients = lpCoeffs;
    for (auto& f : formantFilters)
        f.coefficients = formantCoeffs;
}

void PITShiftPrimeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void PITShiftPrimeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
PITShiftPrimeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramSemitones, "Semitones",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramCents, "Cents",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramFormant, "Formant",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramHPF, "HPF",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.45f), 80.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramLPF, "LPF",
        juce::NormalisableRange<float> (1000.0f, 18000.0f, 0.01f, 0.45f), 14000.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        paramMode, "Mode", juce::StringArray { "Clean", "Dirty" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        paramMix, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    return { params.begin(), params.end() };
}

juce::AudioProcessorEditor* PITShiftPrimeAudioProcessor::createEditor()
{
    return new PITShiftPrimeAudioProcessorEditor (*this);
}

//==============================================================================
PITShiftPrimeAudioProcessorEditor::PITShiftPrimeAudioProcessorEditor (PITShiftPrimeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (semitoneSlider, "Semitones");
    initSlider (centsSlider,    "Cents");
    initSlider (formantSlider,  "Formant");
    initSlider (hpfSlider,      "HPF");
    initSlider (lpfSlider,      "LPF");
    initSlider (mixSlider,      "Mix");

    modeBox.addItemList ({ "Clean", "Dirty" }, 1);
    addAndMakeVisible (modeBox);

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processorRef.getValueTreeState(), paramSemitones, semitoneSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processorRef.getValueTreeState(), paramCents,     centsSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processorRef.getValueTreeState(), paramFormant,   formantSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processorRef.getValueTreeState(), paramHPF,       hpfSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processorRef.getValueTreeState(), paramLPF,       lpfSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processorRef.getValueTreeState(), paramMix,       mixSlider));
    modeAttachment = std::make_unique<ComboAttachment> (processorRef.getValueTreeState(), paramMode, modeBox);

    setSize (720, 320);
}

void PITShiftPrimeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("PIT Shift Prime", getLocalBounds().removeFromTop (30),
                      juce::Justification::centred, 1);
}

void PITShiftPrimeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (30);
    modeBox.setBounds (area.removeFromTop (30).reduced (6));

    auto row1 = area.removeFromTop (120);
    auto row2 = area.removeFromTop (120);
    auto width = row1.getWidth() / 3;
    semitoneSlider.setBounds (row1.removeFromLeft (width).reduced (6));
    centsSlider   .setBounds (row1.removeFromLeft (width).reduced (6));
    formantSlider .setBounds (row1.removeFromLeft (width).reduced (6));

    hpfSlider.setBounds (row2.removeFromLeft (width).reduced (6));
    lpfSlider.setBounds (row2.removeFromLeft (width).reduced (6));
    mixSlider.setBounds (row2.removeFromLeft (width).reduced (6));
}

void PITShiftPrimeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PITShiftPrimeAudioProcessor();
}
