#include "GRDSubHarmForgeAudioProcessor.h"

GRDSubHarmForgeAudioProcessor::GRDSubHarmForgeAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUB_HARM_FORGE", createParameterLayout())
{
}

void GRDSubHarmForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
}

void GRDSubHarmForgeAudioProcessor::releaseResources()
{
}

void GRDSubHarmForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float depth     = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float crossover = juce::jlimit (40.0f, 140.0f, get ("crossover"));
    const float drive     = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float blend     = juce::jlimit (0.0f, 1.0f, get ("blend"));
    const float trimDb    = juce::jlimit (-12.0f, 12.0f, get ("output_trim"));
    const float trimGain  = juce::Decibels::decibelsToGain (trimDb);

    ensureStateSize (numChannels);
    updateFilters (crossover);

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer, true);

    const float subGain = depth * 0.8f;
    const float driveGain = 1.0f + drive * 6.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = dry.getSample (ch, i);
            const float low = state.lowFilter.processSample (input);
            const float sub = state.subFilter.processSample (input);
            const float rectified = std::abs (sub);
            const float synth = std::sin (subPhase) * rectified;

            const float forged = std::tanh ((low + synth * subGain) * driveGain);
            data[i] = juce::jmap (blend, input, forged) * trimGain;

            subPhase += (float) (juce::MathConstants<double>::twoPi * (currentSampleRate > 0.0 ? crossover / currentSampleRate : 0.0));
            if (subPhase > juce::MathConstants<float>::twoPi)
                subPhase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void GRDSubHarmForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDSubHarmForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDSubHarmForgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth", "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("crossover", "Crossover",
                                                                   juce::NormalisableRange<float> (40.0f, 140.0f, 0.01f, 0.4f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("blend", "Blend",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void GRDSubHarmForgeAudioProcessor::ensureStateSize (int numChannels)
{
    if ((int) channelState.size() < numChannels)
        channelState.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (filterSpecSampleRate, currentSampleRate)
                             || filterSpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& state : channelState)
        {
            state.lowFilter.prepare (spec);
            state.lowFilter.reset();
            state.subFilter.prepare (spec);
            state.subFilter.reset();
        }
        filterSpecSampleRate = currentSampleRate;
        filterSpecBlockSize  = targetBlock;
    }
}

void GRDSubHarmForgeAudioProcessor::updateFilters (float crossoverHz)
{
    if (currentSampleRate <= 0.0)
        return;

    auto low = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                 juce::jlimit (40.0f, 180.0f, crossoverHz), 0.7f);
    auto sub = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                 juce::jlimit (30.0f, 100.0f, crossoverHz * 0.5f), 0.8f);
    for (auto& state : channelState)
    {
        state.lowFilter.coefficients = low;
        state.subFilter.coefficients = sub;
    }
}

GRDSubHarmForgeAudioProcessorEditor::GRDSubHarmForgeAudioProcessorEditor (GRDSubHarmForgeAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };
    make (depthSlider,    "Depth");
    make (crossoverSlider,"Crossover");
    make (driveSlider,    "Drive");
    make (blendSlider,    "Blend");
    make (trimSlider,     "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "depth", "crossover", "drive", "blend", "output_trim" };
    juce::Slider* sliders[] = { &depthSlider, &crossoverSlider, &driveSlider, &blendSlider, &trimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 260);
}

void GRDSubHarmForgeAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GRDSubHarmForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Sub Harm Forge", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GRDSubHarmForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    depthSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    crossoverSlider.setBounds (area.removeFromLeft (width).reduced (8));
    driveSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    blendSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    trimSlider    .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GRDSubHarmForgeAudioProcessor::createEditor()
{
    return new GRDSubHarmForgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDSubHarmForgeAudioProcessor();
}
