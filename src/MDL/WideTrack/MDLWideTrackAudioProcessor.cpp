#include "MDLWideTrackAudioProcessor.h"

MDLWideTrackAudioProcessor::MDLWideTrackAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WIDE_TRACK", createParameterLayout())
{
}

void MDLWideTrackAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = juce::jmax (sampleRate, 44100.0);
}

void MDLWideTrackAudioProcessor::releaseResources()
{
}

void MDLWideTrackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalIn  = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float width     = juce::jlimit (0.0f, 2.0f, get ("width"));
    const float spreadMs  = juce::jlimit (0.0f, 5.0f, get ("delay_spread"));
    const float hfPreserve= juce::jlimit (0.0f, 1.0f, get ("hf_preserve"));
    const float monoSafe  = juce::jlimit (0.0f, 1.0f, get ("mono_safe"));

    const int numSamples = buffer.getNumSamples();
    dryBuffer.makeCopyOf (buffer, true);

    if (buffer.getNumChannels() >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (left[i] + right[i]);
            const float side = 0.5f * (left[i] - right[i]);

            const float widenedSide = side * width;
            left[i]  = mid + widenedSide;
            right[i] = mid - widenedSide;
        }

        applyMicroDelay (buffer, spreadMs);

        const float hfGain = juce::jmap (hfPreserve, 0.0f, 1.0f, 0.8f, 1.0f);
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = juce::dsp::FastMathApproximations::tanh (left[i] * hfGain);
            right[i] = juce::dsp::FastMathApproximations::tanh (right[i] * hfGain);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = 0.5f * (dryBuffer.getSample (0, i) + dryBuffer.getSample (1, i));
            left[i]  = juce::jmap (monoSafe, left[i], mono);
            right[i] = juce::jmap (monoSafe, right[i], mono);
        }
    }
}

void MDLWideTrackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLWideTrackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLWideTrackAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width",       "Width",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_spread","Delay Spread",
                                                                   juce::NormalisableRange<float> (0.0f, 5.0f, 0.001f), 1.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hf_preserve", "HF Preserve",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mono_safe",   "Mono Safe",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));

    return { params.begin(), params.end() };
}

MDLWideTrackAudioProcessorEditor::MDLWideTrackAudioProcessorEditor (MDLWideTrackAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (widthSlider,      "Width");
    make (delaySpreadSlider,"Spread");
    make (hfSlider,         "HF Preserve");
    make (monoSlider,       "Mono Safe");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "width", "delay_spread", "hf_preserve", "mono_safe" };
    juce::Slider* sliders[] = { &widthSlider, &delaySpreadSlider, &hfSlider, &monoSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (640, 260);
}

void MDLWideTrackAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (name);
    addAndMakeVisible (slider);
}

void MDLWideTrackAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Wide Track", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLWideTrackAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 4;

    widthSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    delaySpreadSlider.setBounds (area.removeFromLeft (width).reduced (8));
    hfSlider        .setBounds (area.removeFromLeft (width).reduced (8));
    monoSlider      .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLWideTrackAudioProcessor::createEditor()
{
    return new MDLWideTrackAudioProcessorEditor (*this);
}

void MDLWideTrackAudioProcessor::applyMicroDelay (juce::AudioBuffer<float>& buffer, float spreadMs)
{
    if (spreadMs <= 0.0f || buffer.getNumChannels() < 2)
        return;

    const int delaySamples = (int) juce::jlimit (1.0f, (double) currentSampleRate * 0.01,
                                                 spreadMs * 0.001f * (float) currentSampleRate);

    juce::AudioBuffer<float> temp;
    temp.makeCopyOf (buffer, true);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dest = buffer.getWritePointer (ch);
        const auto* src = temp.getReadPointer (ch);

        const int offset = (ch == 0 ? -delaySamples : delaySamples);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const int idx = juce::jlimit (0, buffer.getNumSamples() - 1, i + offset);
            dest[i] = src[idx];
        }
    }
}
