#include "EQFormSetAudioProcessor.h"

EQFormSetAudioProcessor::EQFormSetAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FORM_SET", createParameterLayout())
{
}

void EQFormSetAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    for (auto& formant : formantFilters)
    {
        formant.filter.prepare (spec);
        formant.filter.reset();
        formant.phase = 0.0f;
    }
}

void EQFormSetAudioProcessor::releaseResources()
{
}

void EQFormSetAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const auto formantFreq  = get ("formant_freq");
    const auto formantWidth = juce::jlimit (0.1f, 2.0f, get ("formant_width"));
    const auto movement     = juce::jlimit (0.0f, 1.0f, get ("movement"));
    const auto intensity    = juce::jlimit (0.0f, 1.0f, get ("intensity"));
    const auto mix          = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.makeCopyOf (buffer, true);

    updateFormantFilters (formantFreq, formantWidth, movement);

    const float modulationDepth = juce::jmap (movement, 0.0f, 300.0f);
    const float intensityGainDb = juce::jmap (intensity, 0.0f, 12.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto* dry  = dryBuffer.getReadPointer (ch);
        auto& formant = formantFilters[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float mod = std::sin (formant.phase);
            const float modInput = data[i] + mod * 0.02f;
            const float modulated = formant.filter.processSample (modInput);
            const float enhanced = modulated * juce::Decibels::decibelsToGain (intensityGainDb * std::abs (mod));
            data[i] = enhanced * mix + dry[i] * (1.0f - mix);

            formant.phase += (juce::MathConstants<float>::twoPi * (formantFreq + modulationDepth * mod))
                             / static_cast<float> (currentSampleRate);
            if (formant.phase > juce::MathConstants<float>::twoPi)
                formant.phase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void EQFormSetAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void EQFormSetAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
EQFormSetAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant_freq",  "Formant Freq",
                                                                   juce::NormalisableRange<float> (200.0f, 4000.0f, 0.01f, 0.4f), 800.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("formant_width", "Formant Width",
                                                                   juce::NormalisableRange<float> (0.1f, 2.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("movement",     "Movement",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("intensity",    "Intensity",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",          "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

EQFormSetAudioProcessorEditor::EQFormSetAudioProcessorEditor (EQFormSetAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label)
    {
        initSlider (slider, label);
    };

    make (formantFreqSlider,  "Formant Freq");
    make (formantWidthSlider, "Formant Width");
    make (movementSlider,     "Movement");
    make (intensitySlider,    "Intensity");
    make (mixSlider,          "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "formant_freq", "formant_width", "movement", "intensity", "mix" };
    juce::Slider* sliders[]      = { &formantFreqSlider, &formantWidthSlider, &movementSlider, &intensitySlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 260);
}

void EQFormSetAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void EQFormSetAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("EQ Form Set", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void EQFormSetAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    formantFreqSlider .setBounds (area.removeFromLeft (width).reduced (8));
    formantWidthSlider.setBounds (area.removeFromLeft (width).reduced (8));
    movementSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    intensitySlider   .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider         .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* EQFormSetAudioProcessor::createEditor()
{
    return new EQFormSetAudioProcessorEditor (*this);
}

void EQFormSetAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    juce::dsp::ProcessSpec spec { currentSampleRate,
                                  lastBlockSize > 0 ? lastBlockSize : 512u,
                                  1 };

    if ((int) formantFilters.size() < numChannels)
    {
        const auto previous = (int) formantFilters.size();
        formantFilters.resize (numChannels);
        for (int ch = previous; ch < numChannels; ++ch)
        {
            formantFilters[ch].filter.prepare (spec);
            formantFilters[ch].filter.reset();
            formantFilters[ch].phase = 0.0f;
        }
    }
}

void EQFormSetAudioProcessor::updateFormantFilters (float baseFreq, float width, float movement)
{
    if (currentSampleRate <= 0.0)
        return;

    const float freq = juce::jlimit (200.0f, (float) (currentSampleRate * 0.45f), baseFreq);
    const float bandwidth = juce::jlimit (0.2f, 5.0f, width * (1.0f + movement));
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, freq, bandwidth);
    for (auto& formant : formantFilters)
        formant.filter.coefficients = coeffs;
}
