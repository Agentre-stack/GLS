#include "MDLChopperTremAudioProcessor.h"

MDLChopperTremAudioProcessor::MDLChopperTremAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHOPPER_TREM", createParameterLayout())
{
    pattern.fill (1.0f);
}

void MDLChopperTremAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    rebuildPattern();
}

void MDLChopperTremAudioProcessor::releaseResources()
{
}

void MDLChopperTremAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float depth   = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float rateVal = juce::jlimit (0.25f, 32.0f, get ("rate"));
    const float smooth  = juce::jlimit (0.0f, 1.0f, get ("smooth"));
    const float hpf     = juce::jlimit (20.0f, 2000.0f, get ("hpf"));
    const float mix     = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    dryBuffer.makeCopyOf (buffer, true);

    const float stepRate = rateVal / 4.0f; // steps per quarter note
    const float samplesPerStep = (float) currentSampleRate * 60.0f / (float) (bpm * stepRate);

    juce::dsp::IIR::Filter<float> hpfFilter;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpf);
    hpfFilter.coefficients = coeffs;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);

        float env = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const int stepIndex = (int) ((phase / juce::MathConstants<float>::twoPi) * (float) pattern.size()) % (int) pattern.size();
            const float stepValue = pattern[stepIndex];
            env = smooth * env + (1.0f - smooth) * stepValue;

            float modulated = dry[i] * (1.0f - depth + depth * env);
            modulated = hpfFilter.processSample (modulated);

            wet[i] = modulated * mix + dry[i] * (1.0f - mix);

            phase += juce::MathConstants<float>::twoPi / samplesPerStep;
            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void MDLChopperTremAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void MDLChopperTremAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

void MDLChopperTremAudioProcessor::rebuildPattern()
{
    pattern.fill (1.0f);

    const auto choice = static_cast<int> (apvts.getRawParameterValue ("pattern")->load());

    if (choice == 1) // Triplet-style emphasis
    {
        for (size_t i = 0; i < pattern.size(); ++i)
            pattern[i] = (i % 3 == 0) ? 1.0f : 0.4f;
    }
    else if (choice == 2) // Gated blocks
    {
        for (size_t i = 0; i < pattern.size(); ++i)
            pattern[i] = (i % 8 < 4) ? 1.0f : 0.0f;
    }
}

juce::AudioProcessorEditor* MDLChopperTremAudioProcessor::createEditor()
{
    return new MDLChopperTremAudioProcessorEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLChopperTremAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth",  "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rate",   "Rate",
                                                                   juce::NormalisableRange<float> (0.25f, 32.0f, 0.001f, 0.4f), 4.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("smooth", "Smooth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hpf",    "HPF",
                                                                   juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.35f), 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",    "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("pattern","Pattern",
                                                                   juce::StringArray { "Straight", "Triplet", "Gate" }, 0));

    return { params.begin(), params.end() };
}

MDLChopperTremAudioProcessorEditor::MDLChopperTremAudioProcessorEditor (MDLChopperTremAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (depthSlider,  "Depth");
    make (rateSlider,   "Rate");
    make (smoothSlider, "Smooth");
    make (hpfSlider,    "HPF");
    make (mixSlider,    "Mix");

    patternBox.addItemList ({ "Straight", "Triplet", "Gate" }, 1);
    addAndMakeVisible (patternBox);

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray sliderIds { "depth", "rate", "smooth", "hpf", "mix" };
    juce::Slider* sliders[] = { &depthSlider, &rateSlider, &smoothSlider, &hpfSlider, &mixSlider };

    for (int i = 0; i < sliderIds.size(); ++i)
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, sliderIds[i], *sliders[i]));

    patternAttachment = std::make_unique<ComboAttachment> (state, "pattern", patternBox);

    setSize (640, 260);
}

void MDLChopperTremAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLChopperTremAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Chopper Trem", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLChopperTremAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    patternBox.setBounds (area.removeFromTop (30));

    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 5;
    depthSlider .setBounds (row.removeFromLeft (width).reduced (8));
    rateSlider  .setBounds (row.removeFromLeft (width).reduced (8));
    smoothSlider.setBounds (row.removeFromLeft (width).reduced (8));
    hpfSlider   .setBounds (row.removeFromLeft (width).reduced (8));
    mixSlider   .setBounds (row.removeFromLeft (width).reduced (8));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDLChopperTremAudioProcessor();
}
