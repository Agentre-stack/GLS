#include "EQDynBandAudioProcessor.h"

EQDynBandAudioProcessor::EQDynBandAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DYN_BAND", createParameterLayout())
{
}

void EQDynBandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& band : band1States)
    {
        band.filter.prepare (spec);
        band.filter.reset();
        band.envelope = 0.0f;
        band.gain = 1.0f;
    }
    for (auto& band : band2States)
    {
        band.filter.prepare (spec);
        band.filter.reset();
        band.envelope = 0.0f;
        band.gain = 1.0f;
    }
}

void EQDynBandAudioProcessor::releaseResources()
{
}

void EQDynBandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto b1Freq   = get ("band1_freq");
    const auto b1Q      = get ("band1_q");
    const auto b1Thresh = get ("band1_thresh");
    const auto b1Range  = get ("band1_range");
    const auto b2Freq   = get ("band2_freq");
    const auto b2Q      = get ("band2_q");
    const auto b2Thresh = get ("band2_thresh");
    const auto b2Range  = get ("band2_range");
    const auto mix      = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        updateBandFilters (band1States[ch], b1Freq, b1Q);
        updateBandFilters (band2States[ch], b2Freq, b2Q);
    }

    const float attackMs = 10.0f;
    const float releaseMs = 120.0f;
    const float attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * (float) currentSampleRate));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& band1 = band1States[ch];
        auto& band2 = band2States[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float input = data[i];

            auto processBand = [&](DynamicBand& band, float thresh, float range)
            {
                const float bandSample = band.filter.processSample (input);
                const float level = std::abs (bandSample) + 1.0e-6f;
                if (level > band.envelope)
                    band.envelope = attackCoeff * band.envelope + (1.0f - attackCoeff) * level;
                else
                    band.envelope = releaseCoeff * band.envelope + (1.0f - releaseCoeff) * level;

                const float envDb = juce::Decibels::gainToDecibels (band.envelope);
                const float gainDb = computeGainDb (envDb, thresh, range);
                const float targetGain = juce::Decibels::decibelsToGain (gainDb);
                band.gain += 0.02f * (targetGain - band.gain);

                return std::pair<float, float> { bandSample, bandSample * band.gain };
            };

            const auto [band1Original, band1Processed] = processBand (band1, b1Thresh, b1Range);
            const auto [band2Original, band2Processed] = processBand (band2, b2Thresh, b2Range);

            data[i] = input + (band1Processed - band1Original) + (band2Processed - band2Original);
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * mix + dry[i] * (1.0f - mix);
    }
}

void EQDynBandAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQDynBandAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQDynBandAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto freqRange   = juce::NormalisableRange<float> (40.0f, 12000.0f, 0.01f, 0.4f);
    auto qRange      = juce::NormalisableRange<float> (0.2f, 10.0f, 0.001f, 0.5f);
    auto threshRange = juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f);
    auto rangeRange  = juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f);

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_freq",   "Band1 Freq",   freqRange,   250.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_q",      "Band1 Q",      qRange,      1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_thresh", "Band1 Thresh", threshRange, -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band1_range",  "Band1 Range",  rangeRange,  -6.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_freq",   "Band2 Freq",   freqRange,   4000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_q",      "Band2 Q",      qRange,      1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_thresh", "Band2 Thresh", threshRange, -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("band2_range",  "Band2 Range",  rangeRange,  -6.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

EQDynBandAudioProcessorEditor::EQDynBandAudioProcessorEditor (EQDynBandAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (band1FreqSlider,   "Band1 Freq");
    make (band1QSlider,      "Band1 Q");
    make (band1ThreshSlider, "Band1 Thresh");
    make (band1RangeSlider,  "Band1 Range");
    make (band2FreqSlider,   "Band2 Freq");
    make (band2QSlider,      "Band2 Q");
    make (band2ThreshSlider, "Band2 Thresh");
    make (band2RangeSlider,  "Band2 Range");
    make (mixSlider,         "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "band1_freq", "band1_q", "band1_thresh", "band1_range",
        "band2_freq", "band2_q", "band2_thresh", "band2_range",
        "mix"
    };

    juce::Slider* sliders[] = {
        &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
        &band2FreqSlider, &band2QSlider, &band2ThreshSlider, &band2RangeSlider,
        &mixSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (780, 360);
}

void EQDynBandAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQDynBandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Dyn Band", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQDynBandAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto rowHeight = area.getHeight() / 3;

    auto top = area.removeFromTop (rowHeight);
    auto width = top.getWidth() / 4;
    band1FreqSlider.setBounds (top.removeFromLeft (width).reduced (8));
    band1QSlider   .setBounds (top.removeFromLeft (width).reduced (8));
    band1ThreshSlider.setBounds (top.removeFromLeft (width).reduced (8));
    band1RangeSlider .setBounds (top.removeFromLeft (width).reduced (8));

    auto mid = area.removeFromTop (rowHeight);
    width = mid.getWidth() / 4;
    band2FreqSlider.setBounds (mid.removeFromLeft (width).reduced (8));
    band2QSlider   .setBounds (mid.removeFromLeft (width).reduced (8));
    band2ThreshSlider.setBounds (mid.removeFromLeft (width).reduced (8));
    band2RangeSlider .setBounds (mid.removeFromLeft (width).reduced (8));

    mixSlider.setBounds (area.reduced (8));
}

juce::AudioProcessorEditor* EQDynBandAudioProcessor::createEditor()
{
    return new EQDynBandAudioProcessorEditor (*this);
}

void EQDynBandAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) band1States.size() < numChannels)
    {
        band1States.resize (numChannels);
        band2States.resize (numChannels);
    }
}

void EQDynBandAudioProcessor::updateBandFilters (DynamicBand& bandState, float freq, float q)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto clampedFreq = juce::jlimit (40.0f, (float) (currentSampleRate * 0.49f), freq);
    const auto clampedQ    = juce::jlimit (0.2f, 10.0f, q);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, clampedFreq, clampedQ);
    bandState.filter.coefficients = coeffs;
}

float EQDynBandAudioProcessor::computeGainDb (float envDb, float threshDb, float rangeDb) const
{
    if (rangeDb >= 0.0f)
    {
        if (envDb >= threshDb)
            return 0.0f;

        const float deficit = juce::jlimit (0.0f, 24.0f, threshDb - envDb);
        return (deficit / 24.0f) * rangeDb;
    }

    if (envDb <= threshDb)
        return 0.0f;

    const float excess = juce::jlimit (0.0f, 24.0f, envDb - threshDb);
    return -(excess / 24.0f) * std::abs (rangeDb);
}
