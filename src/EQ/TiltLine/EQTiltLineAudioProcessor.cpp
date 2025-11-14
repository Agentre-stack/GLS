#include "EQTiltLineAudioProcessor.h"

EQTiltLineAudioProcessor::EQTiltLineAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EQ_TILT_LINE", createParameterLayout())
{
}

void EQTiltLineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureFilterState (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& filter : lowShelves)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : highShelves)
    {
        filter.prepare (spec);
        filter.reset();
    }
}

void EQTiltLineAudioProcessor::releaseResources()
{
}

void EQTiltLineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto tiltDb       = get ("tilt");
    const auto pivotFreq    = get ("pivot_freq");
    const auto lowShelfDb   = get ("low_shelf");
    const auto highShelfDb  = get ("high_shelf");
    const auto outputTrimDb = get ("output_trim");

    const float lowGainEffective  = lowShelfDb  - (tiltDb * 0.5f);
    const float highGainEffective = highShelfDb + (tiltDb * 0.5f);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureFilterState (numChannels);
    updateShelves (pivotFreq, lowGainEffective, highGainEffective);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& low = lowShelves[ch];
        auto& high = highShelves[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            sample = low.processSample (sample);
            sample = high.processSample (sample);
            data[i] = sample;
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (outputTrimDb));
}

void EQTiltLineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQTiltLineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQTiltLineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt",       "Tilt",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pivot_freq", "Pivot Freq",
                                                                   juce::NormalisableRange<float> (150.0f, 6000.0f, 0.01f, 0.4f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_shelf",  "Low Shelf",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_shelf", "High Shelf",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim","Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

EQTiltLineAudioProcessorEditor::EQTiltLineAudioProcessorEditor (EQTiltLineAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (tiltSlider,       "Tilt");
    initSlider (pivotSlider,      "Pivot");
    initSlider (lowShelfSlider,   "Low Shelf");
    initSlider (highShelfSlider,  "High Shelf");
    initSlider (outputSlider,     "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "tilt", "pivot_freq", "low_shelf", "high_shelf", "output_trim" };
    juce::Slider* sliders[]      = { &tiltSlider, &pivotSlider, &lowShelfSlider, &highShelfSlider, &outputSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (620, 260);
}

void EQTiltLineAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void EQTiltLineAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Tilt Line", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQTiltLineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    tiltSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    pivotSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    lowShelfSlider .setBounds (area.removeFromLeft (width).reduced (8));
    highShelfSlider.setBounds (area.removeFromLeft (width).reduced (8));
    outputSlider   .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQTiltLineAudioProcessor::createEditor()
{
    return new EQTiltLineAudioProcessorEditor (*this);
}

void EQTiltLineAudioProcessor::ensureFilterState (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) lowShelves.size() < numChannels)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        const auto previous = (int) lowShelves.size();
        lowShelves.resize (numChannels);
        highShelves.resize (numChannels);

        for (int ch = previous; ch < numChannels; ++ch)
        {
            lowShelves[ch].prepare (spec);
            lowShelves[ch].reset();
            highShelves[ch].prepare (spec);
            highShelves[ch].reset();
        }
    }
}

void EQTiltLineAudioProcessor::updateShelves (float pivotFreq, float lowGainDb, float highGainDb)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto freq = juce::jlimit (100.0f, (float) (currentSampleRate * 0.45f), pivotFreq);
    const auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, freq, 0.707f,
                                                                                juce::Decibels::decibelsToGain (lowGainDb));
    const auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, freq, 0.707f,
                                                                                juce::Decibels::decibelsToGain (highGainDb));

    for (auto& filter : lowShelves)
        filter.coefficients = lowCoeffs;
    for (auto& filter : highShelves)
        filter.coefficients = highCoeffs;
}
