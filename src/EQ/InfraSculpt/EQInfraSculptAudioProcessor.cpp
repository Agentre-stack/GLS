#include "EQInfraSculptAudioProcessor.h"

EQInfraSculptAudioProcessor::EQInfraSculptAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "INFRA_SCULPT", createParameterLayout())
{
}

void EQInfraSculptAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels(), 4);
}

void EQInfraSculptAudioProcessor::releaseResources()
{
}

void EQInfraSculptAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto subHpf      = get ("sub_hpf");
    const auto infraSlope  = get ("infra_slope");
    const auto subResonance= get ("sub_resonance");
    const auto monoBelow   = get ("mono_below");
    const auto outputTrim  = get ("output_trim");

    const int stageCount = juce::jlimit (1, 8, (int) std::round (infraSlope / 6.0f));
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);

    ensureStateSize (numChannels, stageCount);
    updateFilters (subHpf, stageCount, subResonance, monoBelow);

    monoBuffer.setSize (numChannels, numSamples, false, false, true);
    monoBuffer.makeCopyOf (buffer, true);

    juce::dsp::AudioBlock<float> block (buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);
        for (int stage = 0; stage < activeStageCount && stage < (int) hpStacks[ch].stages.size(); ++stage)
        {
            juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);
            hpStacks[ch].stages[stage].process (ctx);
        }

        juce::dsp::ProcessContextReplacing<float> resCtx (channelBlock);
        resonanceFilters[ch].process (resCtx);
    }

    // Mono below threshold
    if (numChannels >= 2)
    {
        juce::dsp::AudioBlock<float> monoBlock (monoBuffer);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto monoChannel = monoBlock.getSingleChannelBlock ((size_t) ch);
            juce::dsp::ProcessContextReplacing<float> ctx (monoChannel);
            monoLowFilters[ch].process (ctx);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = 0.0f;
            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
                monoSample += monoBuffer.getReadPointer (ch)[i];
            monoSample *= 0.5f;

            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                float lowOnly = monoBuffer.getReadPointer (ch)[i];
                data[i] += (monoSample - lowOnly);
            }
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (outputTrim));
}

void EQInfraSculptAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQInfraSculptAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQInfraSculptAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_hpf",      "Sub HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 80.0f, 0.01f, 0.4f), 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("infra_slope",  "Infra Slope",
                                                                   juce::NormalisableRange<float> (6.0f, 48.0f, 6.0f), 24.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sub_resonance","Sub Resonance",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mono_below",   "Mono Below",
                                                                   juce::NormalisableRange<float> (40.0f, 200.0f, 0.01f, 0.35f), 90.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim",  "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

EQInfraSculptAudioProcessorEditor::EQInfraSculptAudioProcessorEditor (EQInfraSculptAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (subHpfSlider,      "Sub HPF");
    make (infraSlopeSlider,  "Infra Slope");
    make (subResonanceSlider,"Resonance");
    make (monoBelowSlider,   "Mono Below");
    make (outputTrimSlider,  "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "sub_hpf", "infra_slope", "sub_resonance", "mono_below", "output_trim" };
    juce::Slider* sliders[]      = { &subHpfSlider, &infraSlopeSlider, &subResonanceSlider, &monoBelowSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (650, 260);
}

void EQInfraSculptAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQInfraSculptAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Infra Sculpt", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQInfraSculptAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    subHpfSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    infraSlopeSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    subResonanceSlider.setBounds (area.removeFromLeft (width).reduced (8));
    monoBelowSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    outputTrimSlider  .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQInfraSculptAudioProcessor::createEditor()
{
    return new EQInfraSculptAudioProcessorEditor (*this);
}

void EQInfraSculptAudioProcessor::ensureStateSize (int numChannels, int stageCount)
{
    if (numChannels <= 0)
        return;

    auto ensureStack = [&](HighPassStack& stack)
    {
        if ((int) stack.stages.size() < stageCount)
        {
            const int current = (int) stack.stages.size();
            stack.stages.resize (stageCount);
            juce::dsp::ProcessSpec spec { currentSampleRate,
                                          lastBlockSize > 0 ? lastBlockSize : 512u,
                                          1 };
            for (int s = current; s < stageCount; ++s)
            {
                stack.stages[s].prepare (spec);
                stack.stages[s].reset();
            }
        }
    };

    if ((int) hpStacks.size() < numChannels)
        hpStacks.resize (numChannels);
    if ((int) resonanceFilters.size() < numChannels)
        resonanceFilters.resize (numChannels);
    if ((int) monoLowFilters.size() < numChannels)
        monoLowFilters.resize (numChannels);

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        ensureStack (hpStacks[ch]);

        resonanceFilters[ch].prepare (spec);
        resonanceFilters[ch].reset();
        monoLowFilters[ch].prepare (spec);
        monoLowFilters[ch].reset();
        for (auto& stage : hpStacks[ch].stages)
        {
            stage.prepare (spec);
            stage.reset();
        }
    }
}

void EQInfraSculptAudioProcessor::updateFilters (float subHpf, int stageCount, float resonance, float monoBelow)
{
    if (currentSampleRate <= 0.0)
        return;

    const auto cutoff = juce::jlimit (20.0f, (float) (currentSampleRate * 0.3f), subHpf);
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, cutoff, 0.707f);
    const float resonanceFreq = cutoff * 1.4f;
    const float resonanceGain = juce::Decibels::decibelsToGain (resonance * 9.0f);
    auto resonanceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate,
                                                                                resonanceFreq,
                                                                                1.0f,
                                                                                resonanceGain);
    const auto monoFreq = juce::jlimit (40.0f, (float) (currentSampleRate * 0.45f), monoBelow);
    auto monoCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, monoFreq, 0.707f);

    activeStageCount = stageCount;
    for (auto& stack : hpStacks)
    {
        for (int stage = 0; stage < stageCount; ++stage)
            stack.stages[stage].coefficients = hpCoeffs;
    }
    for (auto& filter : resonanceFilters)
        filter.coefficients = resonanceCoeffs;
    for (auto& filter : monoLowFilters)
        filter.coefficients = monoCoeffs;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQInfraSculptAudioProcessor();
}
