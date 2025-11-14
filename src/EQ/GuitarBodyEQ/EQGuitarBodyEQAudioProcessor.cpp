#include "EQGuitarBodyEQAudioProcessor.h"

EQGuitarBodyEQAudioProcessor::EQGuitarBodyEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GUITAR_BODY_EQ", createParameterLayout())
{
}

void EQGuitarBodyEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

    prepareVector (bodyFilters);
    prepareVector (mudFilters);
    prepareVector (pickFilters);
    prepareVector (airFilters);
}

void EQGuitarBodyEQAudioProcessor::releaseResources()
{
}

void EQGuitarBodyEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto bodyFreq   = get ("body_freq");
    const auto bodyGain   = get ("body_gain");
    const auto mudCutFreq = get ("mud_cut");
    const auto pickAttack = get ("pick_attack");
    const auto airLift    = get ("air_lift");

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureFilterState (numChannels);
    updateFilters (bodyFreq, bodyGain, mudCutFreq, pickAttack, airLift);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers()[ch], 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        bodyFilters[ch].process (ctx);
        mudFilters[ch].process (ctx);
        pickFilters[ch].process (ctx);
        airFilters[ch].process (ctx);
    }
}

void EQGuitarBodyEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQGuitarBodyEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQGuitarBodyEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("body_freq",   "Body Freq",
                                                                   juce::NormalisableRange<float> (80.0f, 500.0f, 0.01f, 0.4f), 180.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("body_gain",   "Body Gain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mud_cut",     "Mud Cut",
                                                                   juce::NormalisableRange<float> (80.0f, 400.0f, 0.01f, 0.4f), 200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pick_attack", "Pick Attack",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("air_lift",    "Air Lift",
                                                                   juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

EQGuitarBodyEQAudioProcessorEditor::EQGuitarBodyEQAudioProcessorEditor (EQGuitarBodyEQAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (bodyFreqSlider,   "Body Freq");
    make (bodyGainSlider,   "Body Gain");
    make (mudCutSlider,     "Mud Cut");
    make (pickAttackSlider, "Pick Attack");
    make (airLiftSlider,    "Air Lift");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "body_freq", "body_gain", "mud_cut", "pick_attack", "air_lift" };
    juce::Slider* sliders[]      = { &bodyFreqSlider, &bodyGainSlider, &mudCutSlider, &pickAttackSlider, &airLiftSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 260);
}

void EQGuitarBodyEQAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQGuitarBodyEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkslategrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Guitar Body", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQGuitarBodyEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    bodyFreqSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    bodyGainSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    mudCutSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    pickAttackSlider .setBounds (area.removeFromLeft (width).reduced (8));
    airLiftSlider    .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQGuitarBodyEQAudioProcessor::createEditor()
{
    return new EQGuitarBodyEQAudioProcessorEditor (*this);
}

void EQGuitarBodyEQAudioProcessor::ensureFilterState (int numChannels)
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

    ensureVector (bodyFilters);
    ensureVector (mudFilters);
    ensureVector (pickFilters);
    ensureVector (airFilters);
}

void EQGuitarBodyEQAudioProcessor::updateFilters (float bodyFreq, float bodyGain,
                                                  float mudCutFreq, float pickGain, float airGain)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto bodyCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate,
                                                                                 juce::jlimit (80.0f, 500.0f, bodyFreq),
                                                                                 0.7f,
                                                                                 juce::Decibels::decibelsToGain (bodyGain));
    const auto mudCoeffs = juce::dsp::IIR::Coefficients<float>::makeNotch (currentSampleRate,
                                                                           juce::jlimit (80.0f, 500.0f, mudCutFreq),
                                                                           1.5f);
    const auto pickCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate,
                                                                                2500.0f,
                                                                                0.7f,
                                                                                juce::Decibels::decibelsToGain (pickGain));
    const auto airCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate,
                                                                               8000.0f,
                                                                               0.7f,
                                                                               juce::Decibels::decibelsToGain (airGain));

    for (auto& filter : bodyFilters)
        filter.coefficients = bodyCoeffs;
    for (auto& filter : mudFilters)
        filter.coefficients = mudCoeffs;
    for (auto& filter : pickFilters)
        filter.coefficients = pickCoeffs;
    for (auto& filter : airFilters)
        filter.coefficients = airCoeffs;
}
