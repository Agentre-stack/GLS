#include "MDLPhaseGridAudioProcessor.h"

MDLPhaseGridAudioProcessor::MDLPhaseGridAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PHASE_GRID", createParameterLayout())
{
}

void MDLPhaseGridAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());
    const auto stageValue = apvts.getRawParameterValue ("stages");
    const auto targetStages = stageValue != nullptr ? (int) stageValue->load() : 6;
    ensureStageState (channels, juce::jlimit (2, 12, targetStages));
}

void MDLPhaseGridAudioProcessor::releaseResources()
{
}

void MDLPhaseGridAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int stages   = juce::jlimit (2, 12, (int) std::round (get ("stages")));
    const float centre = juce::jlimit (200.0f, 8000.0f, get ("center_freq"));
    const float rate   = juce::jlimit (0.01f, 5.0f, get ("rate"));
    const float depth  = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float feedback = juce::jlimit (-0.95f, 0.95f, get ("feedback"));
    const float mix = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    ensureStageState (numChannels, stages);
    updateStageCoefficients (centre, depth, rate);

    if ((int) lfoPhase.size() < numChannels)
        lfoPhase.resize (numChannels, 0.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);

        float& phase = lfoPhase[ch];
        auto& stageChain = channelStages[ch];
        float fbSample = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = wet[i] + fbSample * feedback;

            for (int s = 0; s < stages; ++s)
                sample = stageChain[s].filter.processSample (sample);

            fbSample = sample;

            wet[i] = sample * mix + buffer.getReadPointer (ch)[i] * (1.0f - mix);

            phase += rate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void MDLPhaseGridAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLPhaseGridAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLPhaseGridAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterInt>   ("stages", "Stages", 2, 12, 6));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("center_freq", "Center Freq",
                                                                   juce::NormalisableRange<float> (200.0f, 8000.0f, 0.01f, 0.4f), 600.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rate", "Rate",
                                                                   juce::NormalisableRange<float> (0.01f, 5.0f, 0.001f, 0.4f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth", "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback",
                                                                   juce::NormalisableRange<float> (-0.95f, 0.95f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLPhaseGridAudioProcessorEditor::MDLPhaseGridAudioProcessorEditor (MDLPhaseGridAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (stagesSlider,  "Stages");
    make (centerSlider,  "Center");
    make (rateSlider,    "Rate");
    make (depthSlider,   "Depth");
    make (feedbackSlider,"Feedback");
    make (mixSlider,     "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "stages", "center_freq", "rate", "depth", "feedback", "mix" };
    juce::Slider* sliders[] = { &stagesSlider, &centerSlider, &rateSlider, &depthSlider, &feedbackSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 260);
}

void MDLPhaseGridAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLPhaseGridAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Phase Grid", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLPhaseGridAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 6;

    stagesSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    centerSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    rateSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    depthSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    feedbackSlider.setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider     .setBounds (area.removeFromLeft (width).reduced (8));
}

void MDLPhaseGridAudioProcessor::ensureStageState (int numChannels, int numStages)
{
    if (numChannels <= 0 || numStages <= 0)
        return;

    if ((int) channelStages.size() < numChannels)
        channelStages.resize (numChannels);

    for (auto& stageVector : channelStages)
    {
        if ((int) stageVector.size() < numStages)
        {
            const auto previous = (int) stageVector.size();
            stageVector.resize (numStages);
        }
    }

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (stageSpecSampleRate, currentSampleRate)
                             || stageSpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& stageVector : channelStages)
            for (auto& stage : stageVector)
            {
                stage.filter.prepare (spec);
                stage.filter.reset();
            }

        stageSpecSampleRate = currentSampleRate;
        stageSpecBlockSize = targetBlock;
    }
}

void MDLPhaseGridAudioProcessor::updateStageCoefficients (float centreFreq, float depth, float rate)
{
    if (currentSampleRate <= 0.0)
        return;

    const float baseFreq = juce::jlimit (50.0f, (float) (currentSampleRate * 0.45f), centreFreq);
    const float modDepth = depth * baseFreq * 0.5f;

    for (int ch = 0; ch < (int) channelStages.size(); ++ch)
    {
        auto& stages = channelStages[ch];
        float phase = (ch < (int) lfoPhase.size()) ? lfoPhase[ch] : 0.0f;
        for (int s = 0; s < (int) stages.size(); ++s)
        {
            const float modAmount = std::sin (phase + s * 0.6f) * modDepth;
            const float freq = juce::jlimit (30.0f, (float) (currentSampleRate * 0.49f), baseFreq + modAmount);
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (currentSampleRate, freq, 1.0f);
            stages[s].filter.coefficients = coeffs;
        }
        if ((int) lfoPhase.size() > ch)
        {
            lfoPhase[ch] += rate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            if (lfoPhase[ch] > juce::MathConstants<float>::twoPi)
                lfoPhase[ch] -= juce::MathConstants<float>::twoPi;
        }
    }
}

juce::AudioProcessorEditor* MDLPhaseGridAudioProcessor::createEditor()
{
    return new MDLPhaseGridAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDLPhaseGridAudioProcessor();
}
