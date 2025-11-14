#include "MDLVibeMorphAudioProcessor.h"

MDLVibeMorphAudioProcessor::MDLVibeMorphAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VIBE_MORPH", createParameterLayout())
{
}

void MDLVibeMorphAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStageState (getTotalNumOutputChannels(),
                      4); // start with 4 stages
}

void MDLVibeMorphAudioProcessor::releaseResources()
{
}

void MDLVibeMorphAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float rate  = juce::jlimit (0.05f, 10.0f, get ("rate"));
    const float depth = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float throb = juce::jlimit (0.0f, 1.0f, get ("throb"));
    const int mode    = (int) std::round (apvts.getRawParameterValue ("mode")->load());
    const float mix   = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    ensureStageState (numChannels, mode == 0 ? 4 : 6);
    dryBuffer.makeCopyOf (buffer, true);

    updateStageCoefficients (rate, depth, throb, mode);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        auto& stages = channelStages[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = dry[i];
            for (auto& stage : stages)
                sample = stage.filter.processSample (sample);

            wet[i] = sample * mix + dry[i] * (1.0f - mix);
        }
    }
}

void MDLVibeMorphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLVibeMorphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLVibeMorphAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rate",  "Rate",
                                                                   juce::NormalisableRange<float> (0.05f, 10.0f, 0.001f, 0.4f), 1.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth", "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("throb", "Throb",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("mode",  "Mode",
                                                                   juce::StringArray { "Vibe", "Chorus" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",   "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLVibeMorphAudioProcessorEditor::MDLVibeMorphAudioProcessorEditor (MDLVibeMorphAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (rateSlider,  "Rate");
    make (depthSlider, "Depth");
    make (throbSlider, "Throb");
    make (mixSlider,   "Mix");

    modeBox.addItemList ({ "Vibe", "Chorus" }, 1);
    addAndMakeVisible (modeBox);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray sliderIds { "rate", "depth", "throb", "mix" };
    juce::Slider* sliders[] = { &rateSlider, &depthSlider, &throbSlider, &mixSlider };

    for (int i = 0; i < sliderIds.size(); ++i)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, sliderIds[i], *sliders[i]));

    modeAttachment = std::make_unique<ComboAttachment> (state, "mode", modeBox);

    setSize (600, 240);
}

void MDLVibeMorphAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLVibeMorphAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Vibe Morph", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLVibeMorphAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    modeBox.setBounds (area.removeFromTop (30));

    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 4;
    rateSlider .setBounds (row.removeFromLeft (width).reduced (8));
    depthSlider.setBounds (row.removeFromLeft (width).reduced (8));
    throbSlider.setBounds (row.removeFromLeft (width).reduced (8));
    mixSlider  .setBounds (row.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLVibeMorphAudioProcessor::createEditor()
{
    return new MDLVibeMorphAudioProcessorEditor (*this);
}

void MDLVibeMorphAudioProcessor::ensureStageState (int numChannels, int numStages)
{
    if (numChannels <= 0 || numStages <= 0)
        return;

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  (juce::uint32) 512,
                                  1 };

    if ((int) channelStages.size() < numChannels)
        channelStages.resize (numChannels);

    for (auto& stages : channelStages)
    {
        if ((int) stages.size() < numStages)
        {
            const int previous = (int) stages.size();
            stages.resize (numStages);
            for (int s = previous; s < numStages; ++s)
            {
                stages[s].filter.prepare (spec);
                stages[s].filter.reset();
            }
        }
    }
}

void MDLVibeMorphAudioProcessor::updateStageCoefficients (float rate, float depth, float throb, int mode)
{
    if (currentSampleRate <= 0.0)
        return;

    const float baseFreq = mode == 0 ? 350.0f : 900.0f;
    for (int ch = 0; ch < (int) channelStages.size(); ++ch)
    {
        auto& stages = channelStages[ch];
        float lfo = lfoPhase[ch % lfoPhase.size()];

        for (int s = 0; s < (int) stages.size(); ++s)
        {
            const float mod = std::sin (lfo + s * 0.3f) * depth;
            const float freq = juce::jlimit (20.0f, (float) (currentSampleRate * 0.45f), baseFreq + mod * baseFreq);
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass (currentSampleRate, freq,
                                                                            1.0f + throb * 0.5f);
            stages[s].filter.coefficients = coeffs;
        }

        lfoPhase[ch % lfoPhase.size()] = lfo + rate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
        if (lfoPhase[ch % lfoPhase.size()] > juce::MathConstants<float>::twoPi)
            lfoPhase[ch % lfoPhase.size()] -= juce::MathConstants<float>::twoPi;
    }
}
