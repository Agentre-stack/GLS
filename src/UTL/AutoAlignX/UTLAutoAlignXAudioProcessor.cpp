#include "UTLAutoAlignXAudioProcessor.h"

UTLAutoAlignXAudioProcessor::UTLAutoAlignXAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AUTO_ALIGN_X", createParameterLayout())
{
}

void UTLAutoAlignXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureDelayState (juce::jmax (1, getTotalNumOutputChannels()));
}

void UTLAutoAlignXAudioProcessor::releaseResources()
{
}

void UTLAutoAlignXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const float delayLeftMs  = juce::jlimit (0.0f, 20.0f, get ("delay_left"));
    const float delayRightMs = juce::jlimit (0.0f, 20.0f, get ("delay_right"));
    const bool invertLeft    = apvts.getRawParameterValue ("invert_left")->load() > 0.5f;
    const bool invertRight   = apvts.getRawParameterValue ("invert_right")->load() > 0.5f;

    ensureDelayState (numChannels);

    const float delayLeftSamples  = delayLeftMs * 0.001f * (float) currentSampleRate;
    const float delayRightSamples = delayRightMs * 0.001f * (float) currentSampleRate;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& delay = channelDelays[ch].delay;
        const bool invert = (ch == 0 ? invertLeft : invertRight);
        const float delaySamples = (ch == 0 ? delayLeftSamples : delayRightSamples);
        delay.setDelay (delaySamples);

        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = data[i];
            const float delayed = delay.popSample (0);
            delay.pushSample (0, drySample);
            data[i] = (invert ? -delayed : delayed);
        }
    }
}

void UTLAutoAlignXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void UTLAutoAlignXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
UTLAutoAlignXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_left", "Delay Left (ms)",
                                                                   juce::NormalisableRange<float> (0.0f, 20.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_right", "Delay Right (ms)",
                                                                   juce::NormalisableRange<float> (0.0f, 20.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("invert_left", "Invert Left", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("invert_right", "Invert Right", false));

    return { params.begin(), params.end() };
}

void UTLAutoAlignXAudioProcessor::ensureDelayState (int numChannels)
{
    if ((int) channelDelays.size() < numChannels)
        channelDelays.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (delaySpecSampleRate, currentSampleRate)
                             || delaySpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& state : channelDelays)
        {
            state.delay.prepare (spec);
            state.delay.reset();
        }
        delaySpecSampleRate = currentSampleRate;
        delaySpecBlockSize  = targetBlock;
    }
}

UTLAutoAlignXAudioProcessorEditor::UTLAutoAlignXAudioProcessorEditor (UTLAutoAlignXAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    initSlider (delayLeftSlider,  "Delay L (ms)");
    initSlider (delayRightSlider, "Delay R (ms)");
    addAndMakeVisible (invertLeftButton);
    addAndMakeVisible (invertRightButton);
    invertLeftButton.setButtonText ("Invert L");
    invertRightButton.setButtonText ("Invert R");

    auto& state = processorRef.getValueTreeState();
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "delay_left", delayLeftSlider));
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, "delay_right", delayRightSlider));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "invert_left", invertLeftButton));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, "invert_right", invertRightButton));

    setSize (520, 200);
}

void UTLAutoAlignXAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void UTLAutoAlignXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("UTL Auto Align X", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void UTLAutoAlignXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (120);

    delayLeftSlider .setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (10));
    delayRightSlider.setBounds (top.reduced (10));

    invertLeftButton .setBounds (area.removeFromLeft (area.getWidth() / 2).reduced (10, 0).removeFromTop (30));
    invertRightButton.setBounds (area.removeFromTop (30).reduced (10, 0));
}

juce::AudioProcessorEditor* UTLAutoAlignXAudioProcessor::createEditor()
{
    return new UTLAutoAlignXAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UTLAutoAlignXAudioProcessor();
}
