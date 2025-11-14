#include "GRDMixHeatAudioProcessor.h"

GRDMixHeatAudioProcessor::GRDMixHeatAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIX_HEAT", createParameterLayout())
{
}

void GRDMixHeatAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    toneFilter.reset();
}

void GRDMixHeatAudioProcessor::releaseResources()
{
}

void GRDMixHeatAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int mode      = (int) std::round (apvts.getRawParameterValue ("mode")->load());
    const float drive   = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float tone    = juce::jlimit (-1.0f, 1.0f, get ("tone"));
    const float mix     = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float output  = juce::jlimit (-12.0f, 12.0f, get ("output"));

    const float toneFreq = juce::jmap (tone, -1.0f, 1.0f, 800.0f, 8000.0f);
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, toneFreq, 0.8f);
    toneFilter.coefficients = coeffs;

    const float driveGain = driveToGain (drive);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float dry = data[i];
            float wet = applySaturation (dry * driveGain, drive, mode);
            wet = toneFilter.processSample (wet);
            data[i] = wet * mix + dry * (1.0f - mix);
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (output));
}

void GRDMixHeatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void GRDMixHeatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorEditor* GRDMixHeatAudioProcessor::createEditor()
{
    return new GRDMixHeatAudioProcessorEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDMixHeatAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice>("mode", "Mode",
                                                                   juce::StringArray { "Clean", "Tape", "Triode" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive",  "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone",   "Tone",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",    "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output", "Output",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

GRDMixHeatAudioProcessorEditor::GRDMixHeatAudioProcessorEditor (GRDMixHeatAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (driveSlider,  "Drive");
    initSlider (toneSlider,   "Tone");
    initSlider (mixSlider,    "Mix");
    initSlider (outputSlider, "Output");

    modeBox.addItemList ({ "Clean", "Tape", "Triode" }, 1);
    addAndMakeVisible (modeBox);

    auto& state = processorRef.getValueTreeState();
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "drive",  driveSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "tone",   toneSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "mix",    mixSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "output", outputSlider));
    modeAttachment = std::make_unique<ComboAttachment> (state, "mode", modeBox);

    setSize (600, 240);
}

void GRDMixHeatAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GRDMixHeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Mix Heat", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GRDMixHeatAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    modeBox.setBounds (area.removeFromTop (30));

    auto row = area.removeFromTop (area.getHeight());
    auto width = row.getWidth() / 4;
    driveSlider .setBounds (row.removeFromLeft (width).reduced (8));
    toneSlider  .setBounds (row.removeFromLeft (width).reduced (8));
    mixSlider   .setBounds (row.removeFromLeft (width).reduced (8));
    outputSlider.setBounds (row.removeFromLeft (width).reduced (8));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDMixHeatAudioProcessor();
}

float GRDMixHeatAudioProcessor::driveToGain (float drive) const
{
    return juce::jmap (drive, 0.0f, 1.0f, 1.0f, 10.0f);
}

float GRDMixHeatAudioProcessor::applySaturation (float sample, float drive, int mode) const
{
    const float clean = juce::dsp::FastMathApproximations::tanh (sample);

    if (mode == 0) return clean;

    if (mode == 1)
    {
        const float tape = sample - (sample * sample * sample) * 0.3f;
        return juce::jmap (drive, 0.0f, 1.0f, clean, tape);
    }

    const float triode = juce::dsp::FastMathApproximations::tanh (sample * 1.5f + sample * sample * sample * 0.2f);
    return juce::jmap (drive, 0.0f, 1.0f, clean, triode);
}
