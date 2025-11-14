#include "DYNTransFixAudioProcessor.h"

DYNTransFixAudioProcessor::DYNTransFixAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRANS_FIX", createParameterLayout())
{
}

void DYNTransFixAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    ensureStateSize();
    for (auto& state : channelStates)
    {
        state.detector = 0.0f;
        state.attackEnv = 0.0f;
        state.sustainEnv = 0.0f;
        state.hfFilter.reset();
        state.lfFilter.reset();
    }
}

void DYNTransFixAudioProcessor::releaseResources()
{
}

void DYNTransFixAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto read = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto attackGain   = juce::Decibels::decibelsToGain (read ("attack"));
    const auto sustainGain  = juce::Decibels::decibelsToGain (read ("sustain"));
    const auto tiltFreq     = read ("tilt_freq");
    const auto tiltAmount   = read ("tilt_amount");
    const int detectMode    = static_cast<int> (apvts.getRawParameterValue ("detect_mode")->load());
    const auto mix          = juce::jlimit (0.0f, 1.0f, read ("mix"));

    ensureStateSize();
    dryBuffer.makeCopyOf (buffer);

    const auto attackCoeff  = std::exp (-1.0f / (0.001f * currentSampleRate));
    const auto sustainCoeff = std::exp (-1.0f / (0.01f * currentSampleRate));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            float detectorSample = sample;

            if (detectMode == 1) // HF focus
                detectorSample = state.hfFilter.processSample (sample);
            else if (detectMode == 2) // LF focus
                detectorSample = state.lfFilter.processSample (sample);

            const float level = std::abs (detectorSample);
            if (level > state.detector)
                state.detector = attackCoeff * state.detector + (1.0f - attackCoeff) * level;
            else
                state.detector = sustainCoeff * state.detector + (1.0f - sustainCoeff) * level;

            const float attackMultiplier = 1.0f + (attackGain - 1.0f) * juce::jlimit (0.0f, 1.0f, state.detector * 2.0f);
            const float sustainMultiplier = 1.0f + (sustainGain - 1.0f) * (1.0f - juce::jlimit (0.0f, 1.0f, state.detector * 2.0f));

            sample *= attackMultiplier * sustainMultiplier;
            sample = applyTilt (sample, tiltFreq, tiltAmount);
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

void DYNTransFixAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void DYNTransFixAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
DYNTransFixAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack",     "Attack",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sustain",    "Sustain",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt_freq",  "Tilt Freq",
                                                                   juce::NormalisableRange<float> (100.0f, 8000.0f, 0.01f, 0.4f), 1200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tilt_amount","Tilt Amount",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("detect_mode","Detect Mode",
                                                                    juce::StringArray { "Wideband", "HF Focus", "LF Focus" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

DYNTransFixAudioProcessorEditor::DYNTransFixAudioProcessorEditor (DYNTransFixAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    initSlider (attackSlider,     "Attack");
    initSlider (sustainSlider,    "Sustain");
    initSlider (tiltFreqSlider,   "Tilt Freq");
    initSlider (tiltAmountSlider, "Tilt Amt");
    initSlider (mixSlider,        "Mix");

    detectModeBox.addItemList ({ "Wideband", "HF Focus", "LF Focus" }, 1);
    addAndMakeVisible (detectModeBox);

    auto& state = processorRef.getValueTreeState();
    attackAttachment     = std::make_unique<SliderAttachment>   (state, "attack",     attackSlider);
    sustainAttachment    = std::make_unique<SliderAttachment>   (state, "sustain",    sustainSlider);
    tiltFreqAttachment   = std::make_unique<SliderAttachment>   (state, "tilt_freq",  tiltFreqSlider);
    tiltAmountAttachment = std::make_unique<SliderAttachment>   (state, "tilt_amount",tiltAmountSlider);
    detectModeAttachment = std::make_unique<ComboBoxAttachment> (state, "detect_mode", detectModeBox);
    mixAttachment        = std::make_unique<SliderAttachment>   (state, "mix",        mixSlider);

    setSize (620, 280);
}

void DYNTransFixAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void DYNTransFixAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("DYN Trans Fix", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void DYNTransFixAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto rowHeight = area.getHeight() / 2;

    auto top = area.removeFromTop (rowHeight);
    attackSlider .setBounds (top.removeFromLeft (top.getWidth() / 3).reduced (8));
    sustainSlider.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (8));
    detectModeBox.setBounds (top.reduced (8).removeFromTop (30));

    auto bottom = area.removeFromTop (rowHeight);
    tiltFreqSlider  .setBounds (bottom.removeFromLeft (bottom.getWidth() / 3).reduced (8));
    tiltAmountSlider.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (8));
    mixSlider       .setBounds (bottom.reduced (8));
}

juce::AudioProcessorEditor* DYNTransFixAudioProcessor::createEditor()
{
    return new DYNTransFixAudioProcessorEditor (*this);
}

void DYNTransFixAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
    {
        channelStates.resize (requiredChannels);
        juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) 512, 1 };
        for (auto& state : channelStates)
        {
            state.hfFilter.prepare (spec);
            state.lfFilter.prepare (spec);
            state.hfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 2000.0f);
            state.lfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, 500.0f);
        }
    }
}

float DYNTransFixAudioProcessor::applyTilt (float sample, float freq, float amount)
{
    const float pivot = juce::jlimit (0.0f, 1.0f, (float) (freq / currentSampleRate));
    const float tilt = amount * 0.5f;
    const float lowGain = 1.0f + tilt;
    const float highGain = 1.0f - tilt;
    return sample * (pivot * highGain + (1.0f - pivot) * lowGain);
}
