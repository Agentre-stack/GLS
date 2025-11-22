#include "MDLWideTrackAudioProcessor.h"

MDLWideTrackAudioProcessor::MDLWideTrackAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WIDE_TRACK", createParameterLayout())
{
}

void MDLWideTrackAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());
    dryBuffer.setSize (channels, (int) lastBlockSize);
    sumDiffBuffer.setSize (2, (int) lastBlockSize);

    const int maxDelay = (int) std::ceil (currentSampleRate * 0.02);
    sideDelay.setMaximumDelayInSamples (juce::jmax (1, maxDelay));
    juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
    sideDelay.prepare (spec);
    sideDelay.reset();
    delaySpecSampleRate = currentSampleRate;
    delaySpecBlockSize = lastBlockSize;
}

void MDLWideTrackAudioProcessor::releaseResources()
{
}

void MDLWideTrackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    const auto getParam = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    float width     = juce::jlimit (0.0f, 2.0f, getParam ("width"));
    const float spreadMs  = juce::jlimit (0.0f, 5.0f, getParam ("delay_spread"));
    const float hfPreserve= juce::jlimit (0.0f, 1.0f, getParam ("hf_preserve"));
    const float monoSafeVal = juce::jlimit (0.0f, 1.0f, getParam ("mono_safe"));
    const float outputTrim  = getParam ("output_trim");
    const bool monoSafe   = monoSafeVal > 0.5f;

    if (monoSafe)
        width = juce::jlimit (0.0f, 1.0f, width);

    const float trimGain = juce::Decibels::decibelsToGain (outputTrim);

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);
    sumDiffBuffer.setSize (2, numSamples, false, false, true);

    if ((int) numChannels >= 2)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float left  = dryBuffer.getSample (0, i);
            const float right = dryBuffer.getSample (1, i);
            const float mid   = 0.5f * (left + right);
            const float side  = 0.5f * (left - right);
            sumDiffBuffer.setSample (0, i, mid);
            sumDiffBuffer.setSample (1, i, side);
        }

        const float delaySamples = juce::jlimit (0.0f, 0.02f * (float) currentSampleRate,
                                                 spreadMs * 0.001f * (float) currentSampleRate);
        if (! juce::approximatelyEqual (delaySpecSampleRate, currentSampleRate)
            || delaySpecBlockSize != lastBlockSize)
        {
            juce::dsp::ProcessSpec spec { currentSampleRate, lastBlockSize, 1 };
            sideDelay.setMaximumDelayInSamples ((int) std::ceil (currentSampleRate * 0.02));
            sideDelay.prepare (spec);
            sideDelay.reset();
            delaySpecSampleRate = currentSampleRate;
            delaySpecBlockSize  = lastBlockSize;
        }
        sideDelay.setDelay (delaySamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = sumDiffBuffer.getSample (0, i);
            float side       = sumDiffBuffer.getSample (1, i) * width;

            const float delayedSide = sideDelay.popSample (0);
            sideDelay.pushSample (0, side);
            side = delayedSide * (1.0f - hfPreserve) + side * hfPreserve;

            buffer.setSample (0, i, (mid + side) * trimGain);
            buffer.setSample (1, i, (mid - side) * trimGain);
        }
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, dryBuffer.getSample (0, i) * trimGain);
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
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

MDLWideTrackAudioProcessorEditor::MDLWideTrackAudioProcessorEditor (MDLWideTrackAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto makeSlider = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    makeSlider (widthSlider,        "Width");
    makeSlider (delaySpreadSlider,  "Delay Spread");
    makeSlider (hfSlider,           "HF Preserve");
    makeSlider (monoSlider,         "Mono Safe");
    makeSlider (outputTrimSlider,   "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "width", "delay_spread", "hf_preserve", "mono_safe", "output_trim" };
    juce::Slider* sliders[] = { &widthSlider, &delaySpreadSlider, &hfSlider, &monoSlider, &outputTrimSlider };

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
    auto width = area.getWidth() / 5;

    widthSlider     .setBounds (area.removeFromLeft (width).reduced (8));
    delaySpreadSlider.setBounds (area.removeFromLeft (width).reduced (8));
    hfSlider        .setBounds (area.removeFromLeft (width).reduced (8));
    monoSlider      .setBounds (area.removeFromLeft (width).reduced (8));
    outputTrimSlider.setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLWideTrackAudioProcessor::createEditor()
{
    return new MDLWideTrackAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDLWideTrackAudioProcessor();
}
