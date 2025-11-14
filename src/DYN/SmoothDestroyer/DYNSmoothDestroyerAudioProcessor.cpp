#include "DYNSmoothDestroyerAudioProcessor.h"

DYNSmoothDestroyerAudioProcessor::DYNSmoothDestroyerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SMOOTH_DESTROYER", createParameterLayout())
{
}

void DYNSmoothDestroyerAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    for (auto& band : band1States)
    {
        band.bandFilter.reset();
        band.envelope = 0.0f;
        band.gain = 1.0f;
    }
    for (auto& band : band2States)
    {
        band.bandFilter.reset();
        band.envelope = 0.0f;
        band.gain = 1.0f;
    }
}

void DYNSmoothDestroyerAudioProcessor::releaseResources()
{
}

void DYNSmoothDestroyerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto b1Freq    = read ("band1_freq");
    const auto b1Q       = read ("band1_q");
    const auto b1Thresh  = read ("band1_thresh");
    const auto b1Range   = read ("band1_range");
    const auto b2Freq    = read ("band2_freq");
    const auto b2Q       = read ("band2_q");
    const auto b2Thresh  = read ("band2_thresh");
    const auto b2Range   = read ("band2_range");
    const auto attackMs  = read ("global_attack");
    const auto releaseMs = read ("global_release");
    const auto mix       = juce::jlimit (0.0f, 1.0f, read ("mix"));

    ensureStateSize();
    dryBuffer.makeCopyOf (buffer, true);

    const auto attackCoeff  = std::exp (-1.0f / (attackMs * 0.001f * currentSampleRate));
    const auto releaseCoeff = std::exp (-1.0f / (releaseMs * 0.001f * currentSampleRate));

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& band1 = band1States[ch];
        auto& band2 = band2States[ch];
        updateBandCoefficients (band1, b1Freq, b1Q);
        updateBandCoefficients (band2, b2Freq, b2Q);

        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            float band1Sample = band1.bandFilter.processSample (sample);
            float band2Sample = band2.bandFilter.processSample (sample);

            auto processBand = [&](DynamicBand& band, float input, float thresh, float range)
            {
                const float level = std::abs (input);
                auto& env = band.envelope;
                if (level > env)
                    env = attackCoeff * env + (1.0f - attackCoeff) * level;
                else
                    env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

                const auto envDb = juce::Decibels::gainToDecibels (juce::jmax (env, 1.0e-6f));
                const auto gainDb = computeBandGain (envDb, thresh, range);
                const auto target = juce::Decibels::decibelsToGain (gainDb);
                band.gain += 0.02f * (target - band.gain);
                return input * band.gain;
            };

            band1Sample = processBand (band1, band1Sample, b1Thresh, b1Range);
            band2Sample = processBand (band2, band2Sample, b2Thresh, b2Range);

            const float processed = sample + band1Sample + band2Sample;
            data[i] = processed;
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

void DYNSmoothDestroyerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNSmoothDestroyerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNSmoothDestroyerAudioProcessor::createParameterLayout()
{
    using AP = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<AP> ("band1_freq",   "Band1 Freq",
                                            juce::NormalisableRange<float> (40.0f, 8000.0f, 0.01f, 0.4f), 250.0f));
    params.push_back (std::make_unique<AP> ("band1_q",      "Band1 Q",
                                            juce::NormalisableRange<float> (0.1f, 10.0f, 0.001f, 0.5f), 1.2f));
    params.push_back (std::make_unique<AP> ("band1_thresh", "Band1 Thresh",
                                            juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f));
    params.push_back (std::make_unique<AP> ("band1_range",  "Band1 Range",
                                            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), -6.0f));
    params.push_back (std::make_unique<AP> ("band2_freq",   "Band2 Freq",
                                            juce::NormalisableRange<float> (200.0f, 18000.0f, 0.01f, 0.4f), 4000.0f));
    params.push_back (std::make_unique<AP> ("band2_q",      "Band2 Q",
                                            juce::NormalisableRange<float> (0.1f, 10.0f, 0.001f, 0.5f), 1.2f));
    params.push_back (std::make_unique<AP> ("band2_thresh", "Band2 Thresh",
                                            juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f));
    params.push_back (std::make_unique<AP> ("band2_range",  "Band2 Range",
                                            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), -6.0f));
    params.push_back (std::make_unique<AP> ("global_attack","Global Attack",
                                            juce::NormalisableRange<float> (1.0f, 200.0f, 0.01f, 0.3f), 15.0f));
    params.push_back (std::make_unique<AP> ("global_release","Global Release",
                                            juce::NormalisableRange<float> (5.0f, 1000.0f, 0.01f, 0.3f), 150.0f));
    params.push_back (std::make_unique<AP> ("mix",          "Mix",
                                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

DYNSmoothDestroyerAudioProcessorEditor::DYNSmoothDestroyerAudioProcessorEditor (DYNSmoothDestroyerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto addSlider = [this](juce::Slider& s, const juce::String& label) { initialiseSlider (s, label); };

    addSlider (band1FreqSlider,   "B1 Freq");
    addSlider (band1QSlider,      "B1 Q");
    addSlider (band1ThreshSlider, "B1 Thresh");
    addSlider (band1RangeSlider,  "B1 Range");
    addSlider (band2FreqSlider,   "B2 Freq");
    addSlider (band2QSlider,      "B2 Q");
    addSlider (band2ThreshSlider, "B2 Thresh");
    addSlider (band2RangeSlider,  "B2 Range");
    addSlider (globalAttackSlider,"Attack");
    addSlider (globalReleaseSlider,"Release");
    addSlider (mixSlider,         "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids {
        "band1_freq", "band1_q", "band1_thresh", "band1_range",
        "band2_freq", "band2_q", "band2_thresh", "band2_range",
        "global_attack", "global_release", "mix"
    };

    juce::Slider* sliders[] = {
        &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
        &band2FreqSlider, &band2QSlider, &band2ThreshSlider, &band2RangeSlider,
        &globalAttackSlider, &globalReleaseSlider, &mixSlider
    };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (840, 360);
}

void DYNSmoothDestroyerAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void DYNSmoothDestroyerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Smooth Destroyer", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNSmoothDestroyerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (area.getHeight() / 2);
    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (topRow, { &band1FreqSlider, &band1QSlider, &band1ThreshSlider, &band1RangeSlider,
                         &band2FreqSlider, &band2QSlider });
    layoutRow (area,   { &band2ThreshSlider, &band2RangeSlider,
                         &globalAttackSlider, &globalReleaseSlider, &mixSlider });
}

juce::AudioProcessorEditor* DYNSmoothDestroyerAudioProcessor::createEditor()
{
    return new DYNSmoothDestroyerAudioProcessorEditor (*this);
}

void DYNSmoothDestroyerAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (band1States.size()) != requiredChannels)
    {
        band1States.resize (requiredChannels);
        band2States.resize (requiredChannels);

        juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) 512, 1 };
        for (auto& band : band1States)
            band.bandFilter.prepare (spec);
        for (auto& band : band2States)
            band.bandFilter.prepare (spec);
    }
}

void DYNSmoothDestroyerAudioProcessor::updateBandCoefficients (DynamicBand& band, float freq, float q)
{
    if (currentSampleRate <= 0.0)
        return;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, freq, q);
    band.bandFilter.coefficients = coeffs;
}

float DYNSmoothDestroyerAudioProcessor::computeBandGain (float levelDb, float threshDb, float rangeDb) const
{
    if (levelDb < threshDb)
        return 0.0f;

    const float over = levelDb - threshDb;
    return juce::jlimit (-std::abs (rangeDb), std::abs (rangeDb), rangeDb * (over / 24.0f));
}
