#include "EQAirGlassAudioProcessor.h"

EQAirGlassAudioProcessor::EQAirGlassAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AIR_GLASS", createParameterLayout())
{
}

void EQAirGlassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& filter : airShelves)
    {
        filter.prepare (spec);
        filter.reset();
    }
    for (auto& filter : harshFilters)
    {
        filter.prepare (spec);
        filter.reset();
    }
    std::fill (harshEnvelopes.begin(), harshEnvelopes.end(), 0.0f);
}

void EQAirGlassAudioProcessor::releaseResources()
{
}

void EQAirGlassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto airFreq       = get ("air_freq");
    const auto airGainDb     = get ("air_gain");
    const auto harmonicBlend = juce::jlimit (0.0f, 1.0f, get ("harmonic_blend"));
    const auto deHarsh       = juce::jlimit (0.0f, 1.0f, get ("deharsh"));
    const auto outputTrimDb  = get ("output_trim");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);

    updateShelfCoefficients (airFreq, airGainDb);
    updateHarshFilters (airFreq * 0.8f);

    const float drive = 1.0f + juce::jlimit (0.0f, 18.0f, airGainDb) / 12.0f;
    const float outputGain = juce::Decibels::decibelsToGain (outputTrimDb);
    const float attackCoeff  = std::exp (-1.0f / (0.0025f * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (0.05f * (float) currentSampleRate));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& shelf = airShelves[ch];
        auto& harsh = harshFilters[ch];
        float& env = harshEnvelopes[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            float airy = shelf.processSample (sample);

            const float harmonic = std::tanh (airy * drive);
            airy = airy * (1.0f - harmonicBlend) + harmonic * harmonicBlend;

            const float harshBand = harsh.processSample (airy);
            const float level = std::abs (harshBand);
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            const float reduction = deHarsh * juce::jlimit (0.0f, 1.0f, env * 8.0f);
            airy -= harshBand * reduction;

            data[i] = airy;
        }
    }

    buffer.applyGain (outputGain);
}

void EQAirGlassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQAirGlassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQAirGlassAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_freq",       "Air Freq",
                                                                   juce::NormalisableRange<float> (6000.0f, 20000.0f, 0.01f, 0.4f), 12000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_gain",      "Air Gain",
                                                                   juce::NormalisableRange<float> (-6.0f, 12.0f, 0.1f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("harmonic_blend","Harmonic Blend",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deharsh",       "DeHarsh",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",   "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

EQAirGlassAudioProcessorEditor::EQAirGlassAudioProcessorEditor (EQAirGlassAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (airFreqSlider,       "Air Freq");
    make (airGainSlider,       "Air Gain");
    make (harmonicBlendSlider, "Blend");
    make (deHarshSlider,       "DeHarsh");
    make (outputTrimSlider,    "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "air_freq", "air_gain", "harmonic_blend", "deharsh", "output_trim" };
    juce::Slider* sliders[]      = { &airFreqSlider, &airGainSlider, &harmonicBlendSlider, &deHarshSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (620, 260);
}

void EQAirGlassAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void EQAirGlassAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Air Glass", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQAirGlassAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    airFreqSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    airGainSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    harmonicBlendSlider.setBounds (area.removeFromLeft (width).reduced (8));
    deHarshSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    outputTrimSlider   .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQAirGlassAudioProcessor::createEditor()
{
    return new EQAirGlassAudioProcessorEditor (*this);
}

void EQAirGlassAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    const auto needsResize = (int) airShelves.size() < numChannels;
    if (needsResize)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        const auto previous = (int) airShelves.size();
        airShelves.resize (numChannels);
        harshFilters.resize (numChannels);
        harshEnvelopes.resize (numChannels, 0.0f);

        for (int ch = previous; ch < numChannels; ++ch)
        {
            airShelves[ch].prepare (spec);
            airShelves[ch].reset();
            harshFilters[ch].prepare (spec);
            harshFilters[ch].reset();
            harshEnvelopes[ch] = 0.0f;
        }
    }
    else
    {
        harshEnvelopes.resize (numChannels, 0.0f);
    }
}

void EQAirGlassAudioProcessor::updateShelfCoefficients (float freq, float gainDb)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clampedFreq = juce::jlimit (4000.0f, (float) (currentSampleRate * 0.49f), freq);
    const auto gainLinear = juce::Decibels::decibelsToGain (gainDb);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, clampedFreq, 0.707f, gainLinear);

    for (auto& filter : airShelves)
        filter.coefficients = coeffs;
}

void EQAirGlassAudioProcessor::updateHarshFilters (float freq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clamped = juce::jlimit (2000.0f, (float) (currentSampleRate * 0.49f), freq);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, clamped, 1.2f);

    for (auto& filter : harshFilters)
        filter.coefficients = coeffs;
}
