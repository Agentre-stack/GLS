#include "EQBusPaintAudioProcessor.h"

EQBusPaintAudioProcessor::EQBusPaintAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BUS_PAINT", createParameterLayout())
{
}

void EQBusPaintAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureFilterState (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    auto prepareVector = [&](auto& vec)
    {
        for (auto& filter : vec)
        {
            filter.prepare (spec);
            filter.reset();
        }
    };

    prepareVector (lowShelves);
    prepareVector (highShelves);
    prepareVector (presenceBells);
    prepareVector (warmthBells);
}

void EQBusPaintAudioProcessor::releaseResources()
{
}

void EQBusPaintAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto lowTilt   = get ("low_tilt");
    const auto highTilt  = get ("high_tilt");
    const auto presence  = get ("presence");
    const auto warmth    = get ("warmth");
    const auto outputDb  = get ("output_trim");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureFilterState (numChannels);
    updateFilters (lowTilt, highTilt, presence, warmth);

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
        lowShelves[ch].process (ctx);
        highShelves[ch].process (ctx);
        presenceBells[ch].process (ctx);
        warmthBells[ch].process (ctx);
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (outputDb));
}

void EQBusPaintAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQBusPaintAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQBusPaintAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_tilt",  "Low Tilt",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("high_tilt", "High Tilt",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence",  "Presence",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("warmth",    "Warmth",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim","Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

EQBusPaintAudioProcessorEditor::EQBusPaintAudioProcessorEditor (EQBusPaintAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (lowTiltSlider,  "Low Tilt");
    make (highTiltSlider, "High Tilt");
    make (presenceSlider, "Presence");
    make (warmthSlider,   "Warmth");
    make (outputSlider,   "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "low_tilt", "high_tilt", "presence", "warmth", "output_trim" };
    juce::Slider* sliders[]      = { &lowTiltSlider, &highTiltSlider, &presenceSlider, &warmthSlider, &outputSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (660, 260);
}

void EQBusPaintAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQBusPaintAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Bus Paint", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQBusPaintAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    lowTiltSlider .setBounds (area.removeFromLeft (width).reduced (8));
    highTiltSlider.setBounds (area.removeFromLeft (width).reduced (8));
    presenceSlider.setBounds (area.removeFromLeft (width).reduced (8));
    warmthSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    outputSlider  .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQBusPaintAudioProcessor::createEditor()
{
    return new EQBusPaintAudioProcessorEditor (*this);
}

void EQBusPaintAudioProcessor::ensureFilterState (int numChannels)
{
    if (numChannels <= 0)
        return;

    auto ensureVector = [&](auto& vec)
    {
        if ((int) vec.size() < numChannels)
        {
            juce::dsp::ProcessSpec spec { currentSampleRate,
                                          lastBlockSize > 0 ? lastBlockSize : 512u,
                                          1 };
            const auto previous = (int) vec.size();
            vec.resize (numChannels);
            for (int ch = previous; ch < numChannels; ++ch)
            {
                vec[ch].prepare (spec);
                vec[ch].reset();
            }
        }
    };

    ensureVector (lowShelves);
    ensureVector (highShelves);
    ensureVector (presenceBells);
    ensureVector (warmthBells);
}

void EQBusPaintAudioProcessor::updateFilters (float lowTilt, float highTilt, float presence, float warmth)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto lowShelfGain  = juce::Decibels::decibelsToGain (lowTilt);
    const auto highShelfGain = juce::Decibels::decibelsToGain (highTilt);
    const auto presenceGain  = juce::Decibels::decibelsToGain (presence);
    const auto warmthGain    = juce::Decibels::decibelsToGain (warmth);

    const float lowShelfFreq  = 150.0f;
    const float highShelfFreq = 6000.0f;
    const float presenceFreq  = 3200.0f;
    const float warmthFreq    = 450.0f;

    auto lowCoeffs   = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, lowShelfFreq, 0.707f, lowShelfGain);
    auto highCoeffs  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, highShelfFreq, 0.707f, highShelfGain);
    auto presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, presenceFreq, 1.0f, presenceGain);
    auto warmthCoeffs   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, warmthFreq, 0.8f, warmthGain);

    for (auto& filter : lowShelves)
        filter.coefficients = lowCoeffs;
    for (auto& filter : highShelves)
        filter.coefficients = highCoeffs;
    for (auto& filter : presenceBells)
        filter.coefficients = presenceCoeffs;
    for (auto& filter : warmthBells)
        filter.coefficients = warmthCoeffs;
}
