#include "EQLowBenderAudioProcessor.h"

EQLowBenderAudioProcessor::EQLowBenderAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LOW_BENDER", createParameterLayout())
{
}

void EQLowBenderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureFilterState (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& filter : subShelves)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : punchFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : lowCuts)
    {
        filter.prepare (spec);
        filter.reset();
    }
}

void EQLowBenderAudioProcessor::releaseResources()
{
}

void EQLowBenderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto subBoost  = get ("sub_boost");
    const auto lowCut    = get ("low_cut");
    const auto punchFreq = get ("punch_freq");
    const auto punchGain = get ("punch_gain");
    const auto tightness = juce::jlimit (0.0f, 1.0f, get ("tightness"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureFilterState (numChannels);
    updateFilters (subBoost, punchFreq, punchGain, lowCut, tightness);

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);

        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
        subShelves[ch].process (ctx);
        punchFilters[ch].process (ctx);
        lowCuts[ch].process (ctx);
    }
}

void EQLowBenderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQLowBenderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQLowBenderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_boost",  "Sub Boost",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("low_cut",    "Low Cut",
                                                                   juce::NormalisableRange<float> (20.0f, 120.0f, 0.01f, 0.4f), 40.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("punch_freq", "Punch Freq",
                                                                   juce::NormalisableRange<float> (60.0f, 400.0f, 0.01f, 0.4f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("punch_gain", "Punch Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tightness",  "Tightness",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

EQLowBenderAudioProcessorEditor::EQLowBenderAudioProcessorEditor (EQLowBenderAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (subBoostSlider,  "Sub Boost");
    make (lowCutSlider,    "Low Cut");
    make (punchFreqSlider, "Punch Freq");
    make (punchGainSlider, "Punch Gain");
    make (tightnessSlider, "Tightness");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "sub_boost", "low_cut", "punch_freq", "punch_gain", "tightness" };
    juce::Slider* sliders[]     = { &subBoostSlider, &lowCutSlider, &punchFreqSlider, &punchGainSlider, &tightnessSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 260);
}

void EQLowBenderAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void EQLowBenderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Low Bender", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQLowBenderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    subBoostSlider .setBounds (area.removeFromLeft (width).reduced (8));
    lowCutSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    punchFreqSlider.setBounds (area.removeFromLeft (width).reduced (8));
    punchGainSlider.setBounds (area.removeFromLeft (width).reduced (8));
    tightnessSlider.setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQLowBenderAudioProcessor::createEditor()
{
    return new EQLowBenderAudioProcessorEditor (*this);
}

void EQLowBenderAudioProcessor::ensureFilterState (int numChannels)
{
    if (numChannels <= 0)
        return;

    const auto ensureVector = [this, numChannels](auto& vec)
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

    ensureVector (subShelves);
    ensureVector (punchFilters);
    ensureVector (lowCuts);
}

void EQLowBenderAudioProcessor::updateFilters (float subBoostDb, float punchFreq, float punchGainDb,
                                               float lowCutFreq, float tightness)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto subFreq = 55.0f;
    const auto subCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate,
                                                                              subFreq,
                                                                              0.707f,
                                                                              juce::Decibels::decibelsToGain (subBoostDb));

    const auto punchQ = juce::jmap (tightness, 0.4f, 2.0f);
    const auto clampedPunchFreq = juce::jlimit (40.0f, (float) (currentSampleRate * 0.45f), punchFreq);
    const auto punchCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate,
                                                                                  clampedPunchFreq,
                                                                                  punchQ,
                                                                                  juce::Decibels::decibelsToGain (punchGainDb));

    const auto hpQ = juce::jmap (tightness, 0.5f, 1.2f);
    const auto clampedLowCut = juce::jlimit (20.0f, (float) (currentSampleRate * 0.45f), lowCutFreq);
    const auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate,
                                                                             clampedLowCut,
                                                                             hpQ);

    for (auto& filter : subShelves)
        filter.coefficients = subCoeffs;
    for (auto& filter : punchFilters)
        filter.coefficients = punchCoeffs;
    for (auto& filter : lowCuts)
        filter.coefficients = hpCoeffs;
}
