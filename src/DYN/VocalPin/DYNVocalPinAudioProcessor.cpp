#include "DYNVocalPinAudioProcessor.h"

DYNVocalPinAudioProcessor::DYNVocalPinAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOCAL_PIN", createParameterLayout())
{
}

void DYNVocalPinAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());
    updateDeEssFilters (6000.0f);
}

void DYNVocalPinAudioProcessor::releaseResources()
{
}

void DYNVocalPinAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto threshDb    = get ("thresh");
    const auto ratio       = juce::jmax (1.0f, get ("ratio"));
    const auto attackMs    = juce::jmax (0.1f, get ("attack"));
    const auto releaseMs   = juce::jmax (0.1f, get ("release"));
    const auto deEssFreq   = get ("deess_freq");
    const auto deEssAmount = juce::jlimit (0.0f, 1.0f, get ("deess_amount"));
    const auto mix         = juce::jlimit (0.0f, 1.0f, get ("mix"));

    dryBuffer.makeCopyOf (buffer, true);
    lastBlockSize = (juce::uint32) juce::jmax (1, buffer.getNumSamples());

    ensureStateSize (buffer.getNumChannels());
    updateDeEssFilters (deEssFreq);

    for (auto& follower : compFollowers)
    {
        follower.setSampleRate (currentSampleRate);
        follower.setTimes (attackMs, releaseMs);
    }

    for (auto& follower : deEssFollowers)
    {
        follower.setSampleRate (currentSampleRate);
        follower.setTimes (juce::jmax (0.1f, attackMs * 0.25f),
                           juce::jmax (1.0f, releaseMs * 0.5f));
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* filter = &deEssFilters[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = data[i];
            float sample = drySample;

            const float env = compFollowers[ch].process (sample) + 1.0e-6f;
            const float envDb = juce::Decibels::gainToDecibels (env);
            sample *= computeGain (envDb, threshDb, ratio);

            const float sibilant = filter->processSample (sample);
            const float essLevel = deEssFollowers[ch].process (sibilant);
            const float essNorm = juce::jlimit (0.0f, 1.0f, essLevel * 8.0f);
            const float essAttenuation = deEssAmount * essNorm;
            sample -= sibilant * essAttenuation;

            data[i] = sample;
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

void DYNVocalPinAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNVocalPinAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData (data, sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNVocalPinAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",     "Threshold",
                                                                   juce::NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -18.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ratio",      "Ratio",
                                                                   juce::NormalisableRange<float> (1.0f, 12.0f, 0.01f, 0.5f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",     "Attack",
                                                                   juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.35f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",    "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 600.0f, 0.01f, 0.35f), 150.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deess_freq", "DeEss Freq",
                                                                   juce::NormalisableRange<float> (2000.0f, 12000.0f, 0.01f, 0.35f), 6000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("deess_amount","DeEss Amount",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

DYNVocalPinAudioProcessorEditor::DYNVocalPinAudioProcessorEditor (DYNVocalPinAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& s, const juce::String& label) { initSlider (s, label); };

    make (threshSlider,     "Thresh");
    make (ratioSlider,      "Ratio");
    make (attackSlider,     "Attack");
    make (releaseSlider,    "Release");
    make (deEssFreqSlider,  "DeEss Freq");
    make (deEssAmountSlider,"DeEss Amt");
    make (mixSlider,        "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "thresh", "ratio", "attack", "release", "deess_freq", "deess_amount", "mix" };
    juce::Slider* sliders[]      = { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                     &deEssFreqSlider, &deEssAmountSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (760, 320);
}

void DYNVocalPinAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void DYNVocalPinAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Vocal Pin", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNVocalPinAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto topRow = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (topRow, { &threshSlider, &ratioSlider, &attackSlider, &releaseSlider });
    layoutRow (area,   { &deEssFreqSlider, &deEssAmountSlider, &mixSlider });
}

juce::AudioProcessorEditor* DYNVocalPinAudioProcessor::createEditor()
{
    return new DYNVocalPinAudioProcessorEditor (*this);
}

void DYNVocalPinAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) compFollowers.size() < numChannels)
    {
        compFollowers.resize (numChannels);
        deEssFollowers.resize (numChannels);
    }

    if ((int) deEssFilters.size() < numChannels)
    {
        const int previousSize = (int) deEssFilters.size();
        deEssFilters.resize (numChannels);

        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        for (int i = previousSize; i < numChannels; ++i)
        {
            deEssFilters[i].prepare (spec);
            deEssFilters[i].reset();
        }
    }
    else
    {
        juce::dsp::ProcessSpec spec { currentSampleRate,
                                      lastBlockSize > 0 ? lastBlockSize : 512u,
                                      1 };
        for (auto& filter : deEssFilters)
            filter.prepare (spec);
    }
}

void DYNVocalPinAudioProcessor::updateDeEssFilters (float freq)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto limitedFreq = juce::jlimit (800.0f, (float) (currentSampleRate * 0.45), freq);
    const auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, limitedFreq, 2.0f);

    for (auto& filter : deEssFilters)
        filter.coefficients = coeffs;
}

float DYNVocalPinAudioProcessor::computeGain (float levelDb, float threshDb, float ratio) const
{
    if (ratio <= 1.0f || levelDb <= threshDb)
        return 1.0f;

    const float over = levelDb - threshDb;
    const float compressed = threshDb + over / ratio;
    return juce::Decibels::decibelsToGain (compressed - levelDb);
}
