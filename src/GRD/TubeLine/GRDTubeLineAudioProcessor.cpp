#include "GRDTubeLineAudioProcessor.h"

GRDTubeLineAudioProcessor::GRDTubeLineAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TUBE_LINE", createParameterLayout())
{
}

void GRDTubeLineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
}

void GRDTubeLineAudioProcessor::releaseResources()
{
}

void GRDTubeLineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const float inputTrim = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, get ("input_trim")));
    const float bias      = juce::jlimit (0.0f, 1.0f, get ("bias"));
    const float character = juce::jlimit (0.0f, 1.0f, get ("character"));
    const float mix       = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float outputTrim= juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, get ("output_trim")));

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer, true);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = dry.getSample (ch, i) * inputTrim;
            const float asym  = input + bias * 0.5f;
            float soft = std::tanh (asym * (1.0f + character * 4.0f));
            float hard = juce::jlimit (-1.0f, 1.0f, asym * (1.0f + character * 8.0f));
            const float shaped = juce::jmap (character, soft, hard);
            data[i] = juce::jmap (mix, dry.getSample (ch, i), shaped) * outputTrim;
        }
    }
}

void GRDTubeLineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDTubeLineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDTubeLineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("input_trim", "Input Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bias", "Bias",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("character", "Character",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

GRDTubeLineAudioProcessorEditor::GRDTubeLineAudioProcessorEditor (GRDTubeLineAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };
    make (inputTrimSlider,   "Input Trim");
    make (biasSlider,        "Bias");
    make (characterSlider,   "Character");
    make (mixSlider,         "Mix");
    make (outputTrimSlider,  "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "input_trim", "bias", "character", "mix", "output_trim" };
    juce::Slider* sliders[] = { &inputTrimSlider, &biasSlider, &characterSlider, &mixSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 260);
}

void GRDTubeLineAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GRDTubeLineAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Tube Line", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GRDTubeLineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 5;

    inputTrimSlider .setBounds (area.removeFromLeft (width).reduced (8));
    biasSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    characterSlider .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider       .setBounds (area.removeFromLeft (width).reduced (8));
    outputTrimSlider.setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GRDTubeLineAudioProcessor::createEditor()
{
    return new GRDTubeLineAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDTubeLineAudioProcessor();
}
