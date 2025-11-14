#include "GLSMixHeadAudioProcessor.h"

GLSMixHeadAudioProcessor::GLSMixHeadAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIX_HEAD", createParameterLayout())
{
}

void GLSMixHeadAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
    toneSmoothingCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 600.0f / (float) currentSampleRate);
    ensureStateSize();
    for (auto& state : channelStates)
        state.toneLowState = 0.0f;
}

void GLSMixHeadAudioProcessor::releaseResources()
{
}

void GLSMixHeadAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto readParam = [this](const juce::String& id)
    {
        return apvts.getRawParameterValue (id)->load();
    };

    const auto drive      = juce::jlimit (0.0f, 1.0f, readParam ("drive"));
    const auto headroom   = readParam ("headroom");
    const auto tone       = juce::jlimit (-1.0f, 1.0f, readParam ("tone"));
    const auto widthParam = juce::jlimit (0.0f, 2.0f, readParam ("width"));
    const auto outputTrim = juce::Decibels::decibelsToGain (readParam ("output_trim"));

    ensureStateSize();

    const auto headroomGain = juce::Decibels::decibelsToGain (-headroom);
    const auto lowTiltDb    = -tone * 4.0f;
    const auto highTiltDb   = tone * 4.0f;
    const auto lowGain      = juce::Decibels::decibelsToGain (lowTiltDb);
    const auto highGain     = juce::Decibels::decibelsToGain (highTiltDb);

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = channelStates[ch];
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];

            sample *= headroomGain;
            sample = applySaturation (sample, drive);

            const float low = processTone (state, sample);
            const float high = sample - low;
            sample = low * lowGain + high * highGain;

            data[i] = sample;
        }
    }

    if (numChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            float mid  = 0.5f * (left[i] + right[i]);
            float side = 0.5f * (left[i] - right[i]) * widthParam;
            left[i]  = mid + side;
            right[i] = mid - side;
        }
    }

    buffer.applyGain (outputTrim);
}

void GLSMixHeadAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GLSMixHeadAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GLSMixHeadAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive",       "Drive",       juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("headroom",    "Headroom",    juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone",        "Tone",        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",       "Width",       juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim", juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

GLSMixHeadAudioProcessorEditor::GLSMixHeadAudioProcessorEditor (GLSMixHeadAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& slider, const juce::String& label)
    {
        initialiseSlider (slider, label);
    };

    makeSlider (driveSlider,      "Drive");
    makeSlider (headroomSlider,   "Headroom");
    makeSlider (toneSlider,       "Tone");
    makeSlider (widthSlider,      "Width");
    makeSlider (outputTrimSlider, "Output");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "drive", "headroom", "tone", "width", "output_trim" };
    juce::Slider* sliders[] = { &driveSlider, &headroomSlider, &toneSlider, &widthSlider, &outputTrimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (520, 260);
}

void GLSMixHeadAudioProcessorEditor::initialiseSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void GLSMixHeadAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GLS Mix Head", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GLSMixHeadAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto columns = area.getWidth() / 5;

    juce::Slider* sliders[] = { &driveSlider, &headroomSlider, &toneSlider, &widthSlider, &outputTrimSlider };
    for (auto* slider : sliders)
        slider->setBounds (area.removeFromLeft (columns).reduced (8));
}

juce::AudioProcessorEditor* GLSMixHeadAudioProcessor::createEditor()
{
    return new GLSMixHeadAudioProcessorEditor (*this);
}

void GLSMixHeadAudioProcessor::ensureStateSize()
{
    const auto requiredChannels = getTotalNumOutputChannels();
    if (static_cast<int> (channelStates.size()) != requiredChannels)
        channelStates.resize (requiredChannels);
}

float GLSMixHeadAudioProcessor::processTone (ChannelState& state, float sample) const
{
    state.toneLowState += toneSmoothingCoeff * (sample - state.toneLowState);
    return state.toneLowState;
}

float GLSMixHeadAudioProcessor::applySaturation (float sample, float drive)
{
    if (drive <= 0.0f)
        return sample;

    const auto driveAmount = juce::jmap (drive, 1.0f, 8.0f);
    const auto saturated = std::tanh (sample * driveAmount);
    return juce::jmap (drive, sample, saturated);
}
