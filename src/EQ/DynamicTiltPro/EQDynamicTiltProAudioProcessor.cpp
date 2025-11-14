#include "EQDynamicTiltProAudioProcessor.h"

EQDynamicTiltProAudioProcessor::EQDynamicTiltProAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DYNAMIC_TILT_PRO", createParameterLayout())
{
}

void EQDynamicTiltProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

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
    std::fill (envelopes.begin(), envelopes.end(), 0.0f);
}

void EQDynamicTiltProAudioProcessor::releaseResources()
{
}

void EQDynamicTiltProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto tiltDb     = get ("tilt");
    const auto pivotFreq  = get ("pivot_freq");
    const auto threshDb   = get ("thresh");
    const auto rangeDb    = get ("range");
    const auto attackMs   = get ("attack");
    const auto releaseMs  = get ("release");
    const auto outputTrim = get ("output_trim");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);

    const float attackSeconds  = juce::jmax (1.0f, attackMs) * 0.001f;
    const float releaseSeconds = juce::jmax (5.0f, releaseMs) * 0.001f;
    const float attackCoeff  = std::exp (-1.0f / (attackSeconds  * (float) currentSampleRate));
    const float releaseCoeff = std::exp (-1.0f / (releaseSeconds * (float) currentSampleRate));

    float combinedEnv = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getReadPointer (ch);
        auto& env = envelopes[ch];
        for (int i = 0; i < numSamples; ++i)
        {
            const float level = std::abs (data[i]) + 1.0e-6f;
            if (level > env)
                env = attackCoeff * env + (1.0f - attackCoeff) * level;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * level;

            combinedEnv = juce::jmax (combinedEnv, env);
        }
    }

    const float envDb = juce::Decibels::gainToDecibels (combinedEnv);
    const float normalized = juce::jlimit (-1.0f, 1.0f, (envDb - threshDb) / 24.0f);
    const float dynamicComponent = normalized * rangeDb;
    const float totalTilt = tiltDb + dynamicComponent;

    updateFilters (totalTilt, pivotFreq);

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
        lowShelves[ch].process (ctx);
        highShelves[ch].process (ctx);
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (outputTrim));
}

void EQDynamicTiltProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQDynamicTiltProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQDynamicTiltProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt",      "Tilt",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pivot_freq","Pivot Freq",
                                                                   juce::NormalisableRange<float> (150.0f, 6000.0f, 0.01f, 0.4f), 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("thresh",    "Threshold",
                                                                   juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("range",     "Range",
                                                                   juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",    "Attack",
                                                                   juce::NormalisableRange<float> (1.0f, 200.0f, 0.01f, 0.35f), 15.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release",   "Release",
                                                                   juce::NormalisableRange<float> (10.0f, 1000.0f, 0.01f, 0.35f), 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim","Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

EQDynamicTiltProAudioProcessorEditor::EQDynamicTiltProAudioProcessorEditor (EQDynamicTiltProAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (tiltSlider,    "Tilt");
    make (pivotSlider,   "Pivot");
    make (threshSlider,  "Thresh");
    make (rangeSlider,   "Range");
    make (attackSlider,  "Attack");
    make (releaseSlider, "Release");
    make (outputSlider,  "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "tilt", "pivot_freq", "thresh", "range", "attack", "release", "output_trim" };
    juce::Slider* sliders[]      = { &tiltSlider, &pivotSlider, &threshSlider, &rangeSlider, &attackSlider, &releaseSlider, &outputSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 300);
}

void EQDynamicTiltProAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQDynamicTiltProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Dynamic Tilt Pro", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQDynamicTiltProAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (area.getHeight() / 2);

    auto layoutRow = [](juce::Rectangle<int> bounds, std::initializer_list<juce::Component*> comps)
    {
        auto width = bounds.getWidth() / static_cast<int> (comps.size());
        for (auto* comp : comps)
            comp->setBounds (bounds.removeFromLeft (width).reduced (8));
    };

    layoutRow (top,  { &tiltSlider, &pivotSlider, &threshSlider, &rangeSlider });
    layoutRow (area, { &attackSlider, &releaseSlider, &outputSlider });
}

juce::AudioProcessorEditor* EQDynamicTiltProAudioProcessor::createEditor()
{
    return new EQDynamicTiltProAudioProcessorEditor (*this);
}

void EQDynamicTiltProAudioProcessor::ensureStateSize (int numChannels)
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
            const int previous = (int) vec.size();
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
    if ((int) envelopes.size() < numChannels)
        envelopes.resize (numChannels, 0.0f);
}

void EQDynamicTiltProAudioProcessor::updateFilters (float totalTiltDb, float pivotFreq)
{
    if (currentSampleRate <= 0.0)
        return;

    const float limitedPivot = juce::jlimit (80.0f, (float) (currentSampleRate * 0.45f), pivotFreq);
    const float halfTilt = juce::jlimit (-18.0f, 18.0f, totalTiltDb) * 0.5f;
    const float lowGain  = juce::Decibels::decibelsToGain (-halfTilt);
    const float highGain = juce::Decibels::decibelsToGain (halfTilt);

    auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, limitedPivot, 0.707f, lowGain);
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, limitedPivot, 0.707f, highGain);

    for (auto& filter : lowShelves)
        filter.coefficients = lowCoeffs;
    for (auto& filter : highShelves)
        filter.coefficients = highCoeffs;
}
