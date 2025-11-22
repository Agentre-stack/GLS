#include "MDLTempoLFOAudioProcessor.h"

MDLTempoLFOAudioProcessor::MDLTempoLFOAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TEMPO_LFO", createParameterLayout())
{
}

void MDLTempoLFOAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lfoPhase = 0.0f;
    smoothedValue = 0.0f;
    refreshTempoFromHost();
}

void MDLTempoLFOAudioProcessor::releaseResources()
{
}

void MDLTempoLFOAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float depth    = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float offset   = juce::jlimit (-1.0f, 1.0f, get ("offset"));
    const float smoothing= juce::jlimit (0.0f, 1.0f, get ("smoothing"));
    const int shape      = (int) std::round (apvts.getRawParameterValue ("shape")->load());

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    refreshTempoFromHost();

    const float phaseIncrement = getSyncRate();
    const float smoothCoeff = std::exp (-juce::MathConstants<float>::twoPi * smoothing / (float) currentSampleRate);

    float phase = lfoPhase;
    float modValue = smoothedValue;

    for (int i = 0; i < numSamples; ++i)
    {
        const float target = getWaveValue (phase, shape);
        modValue = smoothCoeff * modValue + (1.0f - smoothCoeff) * target;

        const float wet = juce::jlimit (-1.0f, 1.0f, offset + depth * modValue);
        const float gain = juce::jlimit (0.0f, 2.0f, 1.0f + wet);

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);

        phase += phaseIncrement;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }

    lfoPhase = phase;
    smoothedValue = modValue;
}

void MDLTempoLFOAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLTempoLFOAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLTempoLFOAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth",  "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("offset", "Offset",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("smoothing", "Smoothing",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("shape", "Shape",
                                                                   juce::StringArray { "Sine", "Triangle", "Square" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("sync",  "Sync",
                                                                   juce::StringArray { "1/1", "1/2", "1/4", "1/8" }, 2));

    return { params.begin(), params.end() };
}

MDLTempoLFOAudioProcessorEditor::MDLTempoLFOAudioProcessorEditor (MDLTempoLFOAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (depthSlider,    "Depth");
    make (offsetSlider,   "Offset");
    make (smoothingSlider,"Smoothing");

    shapeBox.addItemList ({ "Sine", "Triangle", "Square" }, 1);
    syncBox.addItemList ({ "1/1", "1/2", "1/4", "1/8" }, 1);
    addAndMakeVisible (shapeBox);
    addAndMakeVisible (syncBox);

    auto& state = processorRef.getValueTreeState();
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "depth", depthSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "offset", offsetSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "smoothing", smoothingSlider));
    shapeAttachment = std::make_unique<ComboAttachment> (state, "shape", shapeBox);
    syncAttachment  = std::make_unique<ComboAttachment> (state, "sync",  syncBox);

    setSize (540, 220);
}

void MDLTempoLFOAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLTempoLFOAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Tempo LFO", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLTempoLFOAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    shapeBox.setBounds (area.removeFromTop (30));
    syncBox.setBounds  (area.removeFromTop (30));

    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 3;
    depthSlider   .setBounds (row.removeFromLeft (width).reduced (8));
    offsetSlider  .setBounds (row.removeFromLeft (width).reduced (8));
    smoothingSlider.setBounds (row.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLTempoLFOAudioProcessor::createEditor()
{
    return new MDLTempoLFOAudioProcessorEditor (*this);
}

float MDLTempoLFOAudioProcessor::getSyncRate() const
{
    const int syncIndex = (int) std::round (apvts.getRawParameterValue ("sync")->load());
    const float noteLength = (syncIndex == 0 ? 1.0f :
                              syncIndex == 1 ? 0.5f :
                              syncIndex == 2 ? 0.25f : 0.125f);
    const float beatsPerSecond = static_cast<float> (bpm / 60.0);
    const float cyclesPerSecond = beatsPerSecond / noteLength;
    return cyclesPerSecond / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
}

float MDLTempoLFOAudioProcessor::getWaveValue (float phase, int shape) const
{
    switch (shape)
    {
        case 1: return juce::jmap (phase / juce::MathConstants<float>::pi,
                                   -1.0f, 1.0f);
        case 2: return phase < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
        default: return std::sin (phase);
    }
}

void MDLTempoLFOAudioProcessor::refreshTempoFromHost()
{
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpmValue = position->getBpm(); bpmValue.hasValue() && *bpmValue > 0.0)
            {
                bpm = *bpmValue;
                return;
            }
        }
    }

    bpm = 120.0;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDLTempoLFOAudioProcessor();
}
