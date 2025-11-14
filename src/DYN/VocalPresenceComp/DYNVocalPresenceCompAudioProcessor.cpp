#include "DYNVocalPresenceCompAudioProcessor.h"

DYNVocalPresenceCompAudioProcessor::DYNVocalPresenceCompAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOCAL_PRESENCE_COMP", createParameterLayout())
{
}

void DYNVocalPresenceCompAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& filter : presenceFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : airFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& follower : presenceFollowers)
        follower.setSampleRate (currentSampleRate);
}

void DYNVocalPresenceCompAudioProcessor::releaseResources()
{
}

void DYNVocalPresenceCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                        juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto presenceFreq = get ("presence_freq");
    const auto presenceQ    = get ("presence_q");
    const auto presenceThresh = get ("presence_thresh");
    const auto range        = get ("range");
    const auto attack       = get ("attack");
    const auto release      = get ("release");
    const auto airGain      = get ("air_gain");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);

    for (auto& follower : presenceFollowers)
    {
        follower.setSampleRate (currentSampleRate);
        follower.setTimes (attack, release);
    }

    updatePresenceFilters (presenceFreq, presenceQ);

    const float presenceThreshDb = presenceThresh;
    const float rangeDb = range;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& filter = presenceFilters[ch];
        auto& follower = presenceFollowers[ch];
        float& gainSmooth = presenceGainSmoothers[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float inputSample = data[i];
            const float bandSample = filter.processSample (inputSample);
            const float env = follower.process (bandSample) + 1.0e-6f;
            const float envDb = juce::Decibels::gainToDecibels (env);
            const float targetGainDb = computePresenceGainDb (envDb, presenceThreshDb, rangeDb);
            const float targetGain = juce::Decibels::decibelsToGain (targetGainDb);
            gainSmooth += 0.02f * (targetGain - gainSmooth);

            const float adjusted = bandSample * gainSmooth;
            data[i] = inputSample + (adjusted - bandSample);
        }
    }

    updateAirFilters (airGain);
    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto context = juce::dsp::ProcessContextReplacing<float> (block.getSingleChannelBlock ((size_t) ch));
        airFilters[ch].process (context);
    }
}

void DYNVocalPresenceCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNVocalPresenceCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNVocalPresenceCompAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence_freq",   "Presence Freq",
                                                                   juce::NormalisableRange<float> (500.0f, 8000.0f, 0.01f, 0.4f), 2500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence_q",     "Presence Q",
                                                                   juce::NormalisableRange<float> (0.2f, 5.0f, 0.001f, 0.5f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("presence_thresh","Presence Thresh",
                                                                   juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -15.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("range",          "Range",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",         "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",        "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 500.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_gain",       "Air Gain",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

DYNVocalPresenceCompAudioProcessorEditor::DYNVocalPresenceCompAudioProcessorEditor (DYNVocalPresenceCompAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (presenceFreqSlider,   "Freq");
    make (presenceQSlider,      "Q");
    make (presenceThreshSlider, "Thresh");
    make (rangeSlider,          "Range");
    make (attackSlider,         "Attack");
    make (releaseSlider,        "Release");
    make (airGainSlider,        "Air");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "presence_freq", "presence_q", "presence_thresh", "range", "attack", "release", "air_gain" };
    juce::Slider* sliders[] = { &presenceFreqSlider, &presenceQSlider, &presenceThreshSlider,
                                &rangeSlider, &attackSlider, &releaseSlider, &airGainSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 300);
}

void DYNVocalPresenceCompAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNVocalPresenceCompAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Vocal Presence Comp", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNVocalPresenceCompAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    auto top = area.removeFromTop (area.getHeight() / 2);
    layoutRow (top, { &presenceFreqSlider, &presenceQSlider, &presenceThreshSlider, &rangeSlider });
    layoutRow (area, { &attackSlider, &releaseSlider, &airGainSlider });
}

juce::AudioProcessorEditor* DYNVocalPresenceCompAudioProcessor::createEditor()
{
    return new DYNVocalPresenceCompAudioProcessorEditor (*this);
}

void DYNVocalPresenceCompAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };

    const bool sizeChanged = (int) presenceFilters.size() < numChannels;

    if (sizeChanged)
    {
        const auto previous = (int) presenceFilters.size();
        presenceFilters.resize (numChannels);
        presenceFollowers.resize (numChannels);
        presenceGainSmoothers.resize (numChannels, 1.0f);
        airFilters.resize (numChannels);

        for (int ch = previous; ch < numChannels; ++ch)
        {
            presenceFilters[ch].prepare (spec);
            presenceFilters[ch].reset();
            presenceFollowers[ch].setSampleRate (currentSampleRate);
            presenceFollowers[ch].reset();
            presenceGainSmoothers[ch] = 1.0f;
            airFilters[ch].prepare (spec);
            airFilters[ch].reset();
        }
    }

    if (sizeChanged)
        return;

    for (auto& follower : presenceFollowers)
        follower.setSampleRate (currentSampleRate);
}

void DYNVocalPresenceCompAudioProcessor::updatePresenceFilters (float freq, float q)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clampedFreq = juce::jlimit (200.0f, (float) (currentSampleRate * 0.45), freq);
    const auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, clampedFreq, juce::jmax (0.1f, q));
    for (auto& filter : presenceFilters)
        filter.coefficients = coeffs;
}

void DYNVocalPresenceCompAudioProcessor::updateAirFilters (float airGainDb)
{
    if (currentSampleRate <= 0.0)
        return;

    const float freq = juce::jlimit (2000.0f, (float) (currentSampleRate * 0.49f), 9500.0f);
    const auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, freq, 0.707f,
                                                                            juce::Decibels::decibelsToGain (airGainDb));
    for (auto& filter : airFilters)
        filter.coefficients = coeffs;
}

float DYNVocalPresenceCompAudioProcessor::computePresenceGainDb (float levelDb, float thresholdDb, float rangeDb) const
{
    if (rangeDb >= 0.0f)
    {
        if (levelDb >= thresholdDb)
            return 0.0f;

        const float deficit = juce::jlimit (0.0f, 24.0f, thresholdDb - levelDb);
        return juce::jlimit (0.0f, rangeDb, (deficit / 24.0f) * rangeDb);
    }

    if (levelDb <= thresholdDb)
        return 0.0f;

    const float excess = juce::jlimit (0.0f, 24.0f, levelDb - thresholdDb);
    return juce::jlimit (rangeDb, 0.0f, -(excess / 24.0f) * std::abs (rangeDb));
}
