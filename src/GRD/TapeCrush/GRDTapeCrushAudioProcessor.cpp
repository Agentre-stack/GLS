#include "GRDTapeCrushAudioProcessor.h"

GRDTapeCrushAudioProcessor::GRDTapeCrushAudioProcessor()
    : DualPrecisionAudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TAPE_CRUSH", createParameterLayout())
{
}

void GRDTapeCrushAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    ensureStateSize (juce::jmax (1, getTotalNumOutputChannels()));
    dryBuffer.setSize (getTotalNumOutputChannels(), (int) lastBlockSize);
}

void GRDTapeCrushAudioProcessor::releaseResources()
{
}

void GRDTapeCrushAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    const float drive   = juce::jlimit (0.0f, 1.0f, get ("drive"));
    const float wow     = juce::jlimit (0.0f, 1.0f, get ("wow"));
    const float flutter = juce::jlimit (0.0f, 1.0f, get ("flutter"));
    const float hiss    = juce::jlimit (0.0f, 1.0f, get ("hiss"));
    const float tone    = juce::jlimit (800.0f, 9000.0f, get ("tone"));
    const float mix     = juce::jlimit (0.0f, 1.0f, get ("mix"));
    const float trim    = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, get ("output_trim")));

    lastBlockSize = (juce::uint32) juce::jmax (1, numSamples);
    ensureStateSize (numChannels);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);
    updateToneFilters (tone);

    juce::Random random;
    const float wowRate = juce::jmap (wow, 0.1f, 0.5f);
    const float flutterRate = juce::jmap (flutter, 3.0f, 10.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& state = channelState[ch];
        state.delay.setDelay (80.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dryBuffer.getSample (ch, i);

            const float wowMod = std::sin (state.wowPhase) * wow * 8.0f;
            const float flutterMod = std::sin (state.flutterPhase) * flutter * 2.0f;
            const float delaySamples = 60.0f + wowMod + flutterMod;
            state.delay.setDelay (juce::jlimit (10.0f, 200.0f, delaySamples));

            float delayed = state.delay.popSample (0);
            const float hissNoise = (random.nextFloat() * 2.0f - 1.0f) * hiss * 0.01f;
            delayed += hissNoise;

            float saturated = std::tanh ((delayed + drySample * 0.3f) * (1.0f + drive * 5.0f));
            saturated = state.toneFilter.processSample (saturated);

            state.delay.pushSample (0, drySample + saturated * 0.4f);

            data[i] = juce::jmap (mix, drySample, saturated) * trim;

            state.wowPhase += wowRate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            state.flutterPhase += flutterRate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            if (state.wowPhase > juce::MathConstants<float>::twoPi) state.wowPhase -= juce::MathConstants<float>::twoPi;
            if (state.flutterPhase > juce::MathConstants<float>::twoPi) state.flutterPhase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void GRDTapeCrushAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void GRDTapeCrushAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
GRDTapeCrushAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive", "Drive",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("wow", "Wow",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("flutter", "Flutter",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("hiss", "Hiss",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.15f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone", "Tone",
                                                                   juce::NormalisableRange<float> (800.0f, 9000.0f, 0.01f, 0.4f), 3500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("output_trim", "Output Trim",
                                                                   juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void GRDTapeCrushAudioProcessor::ensureStateSize (int numChannels)
{
    if ((int) channelState.size() < numChannels)
        channelState.resize ((size_t) numChannels);

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (specSampleRate, currentSampleRate)
                             || specBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& state : channelState)
        {
            state.delay.prepare (spec);
            state.delay.reset();
            state.toneFilter.prepare (spec);
            state.toneFilter.reset();
        }
        specSampleRate = currentSampleRate;
        specBlockSize = targetBlock;
    }
}

void GRDTapeCrushAudioProcessor::updateToneFilters (float tone)
{
    if (currentSampleRate <= 0.0)
        return;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate,
                                                                    juce::jlimit (500.0f, (float) (currentSampleRate * 0.45f), tone), 0.7f);
    for (auto& state : channelState)
        state.toneFilter.coefficients = coeffs;
}

GRDTapeCrushAudioProcessorEditor::GRDTapeCrushAudioProcessorEditor (GRDTapeCrushAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor), processorRef (processor)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };
    make (driveSlider,   "Drive");
    make (wowSlider,     "Wow");
    make (flutterSlider, "Flutter");
    make (hissSlider,    "Hiss");
    make (toneSlider,    "Tone");
    make (mixSlider,     "Mix");
    make (trimSlider,    "Output Trim");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "drive", "wow", "flutter", "hiss", "tone", "mix", "output_trim" };
    juce::Slider* sliders[] = { &driveSlider, &wowSlider, &flutterSlider, &hissSlider, &toneSlider, &mixSlider, &trimSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (760, 300);
}

void GRDTapeCrushAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void GRDTapeCrushAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("GRD Tape Crush", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void GRDTapeCrushAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 7;

    driveSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    wowSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    flutterSlider.setBounds (area.removeFromLeft (width).reduced (8));
    hissSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    toneSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    trimSlider   .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* GRDTapeCrushAudioProcessor::createEditor()
{
    return new GRDTapeCrushAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GRDTapeCrushAudioProcessor();
}
