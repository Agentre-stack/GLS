#include "MDLFlangerJetAudioProcessor.h"

MDLFlangerJetAudioProcessor::MDLFlangerJetAudioProcessor()
    : DualPrecisionAudioProcessor(BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FLANGER_JET", createParameterLayout())
{
}

void MDLFlangerJetAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());
    dryBuffer.setSize (channels, (int) lastBlockSize);
    ensureStateSize (channels);
    updateDelayBounds();
}

void MDLFlangerJetAudioProcessor::releaseResources()
{
}

void MDLFlangerJetAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto get = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float delayBase = juce::jlimit (0.1f, 10.0f, get ("delay_base"));
    const float depth     = juce::jlimit (0.0f, 1.0f, get ("depth"));
    const float rate      = juce::jlimit (0.01f, 5.0f, get ("rate"));
    const float feedback  = juce::jlimit (-0.95f, 0.95f, get ("feedback"));
    const float manual    = juce::jlimit (-1.0f, 1.0f, get ("manual"));
    const float mix       = juce::jlimit (0.0f, 1.0f, get ("mix"));

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    ensureStateSize (numChannels);
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    dryBuffer.makeCopyOf (buffer, true);
    updateDelayBounds();

    const float baseSamples = delayBase * 0.001f * (float) currentSampleRate;
    const float depthSamples = depth * currentSampleRate * 0.002f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        auto& line = lines[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float lfo = std::sin (line.lfoPhase) + manual;
            const float modDelay = baseSamples + depthSamples * lfo;
            line.delay.setDelay (juce::jlimit (1.0f, (float) (currentSampleRate * 0.02f), modDelay));

            const float delayed = line.delay.popSample (0);
            const float feed = delayed * feedback + dry[i];
            line.delay.pushSample (0, feed);

            wet[i] = delayed * mix + dry[i] * (1.0f - mix);

            line.lfoPhase += rate / (float) currentSampleRate * juce::MathConstants<float>::twoPi;
            if (line.lfoPhase > juce::MathConstants<float>::twoPi)
                line.lfoPhase -= juce::MathConstants<float>::twoPi;
        }
    }
}

void MDLFlangerJetAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
}

void MDLFlangerJetAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, sizeInBytes); tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MDLFlangerJetAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delay_base", "Delay Base",
                                                                   juce::NormalisableRange<float> (0.1f, 10.0f, 0.001f, 0.4f), 1.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth",      "Depth",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rate",       "Rate",
                                                                   juce::NormalisableRange<float> (0.01f, 5.0f, 0.001f, 0.4f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback",   "Feedback",
                                                                   juce::NormalisableRange<float> (-0.95f, 0.95f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("manual",     "Manual",
                                                                   juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

MDLFlangerJetAudioProcessorEditor::MDLFlangerJetAudioProcessorEditor (MDLFlangerJetAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    auto make = [this](juce::Slider& slider, const juce::String& label) { initSlider (slider, label); };

    make (delaySlider,    "Delay");
    make (depthSlider,    "Depth");
    make (rateSlider,     "Rate");
    make (feedbackSlider, "Feedback");
    make (manualSlider,   "Manual");
    make (mixSlider,      "Mix");

    auto& state = processorRef.getValueTreeState();
    const juce::StringArray ids { "delay_base", "depth", "rate", "feedback", "manual", "mix" };
    juce::Slider* sliders[] = { &delaySlider, &depthSlider, &rateSlider, &feedbackSlider, &manualSlider, &mixSlider };

    for (int i = 0; i < ids.size(); ++i)
        attachments.push_back (std::make_unique<SliderAttachment> (state, ids[i], *sliders[i]));

    setSize (720, 260);
}

void MDLFlangerJetAudioProcessorEditor::initSlider (juce::Slider& slider, const juce::String& label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setName (label);
    addAndMakeVisible (slider);
}

void MDLFlangerJetAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("MDL Flanger Jet", getLocalBounds().removeFromTop (24), juce::Justification::centred, 1);
}

void MDLFlangerJetAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto width = area.getWidth() / 6;

    delaySlider   .setBounds (area.removeFromLeft (width).reduced (8));
    depthSlider   .setBounds (area.removeFromLeft (width).reduced (8));
    rateSlider    .setBounds (area.removeFromLeft (width).reduced (8));
    feedbackSlider.setBounds (area.removeFromLeft (width).reduced (8));
    manualSlider  .setBounds (area.removeFromLeft (width).reduced (8));
    mixSlider     .setBounds (area.removeFromLeft (width).reduced (8));
}

juce::AudioProcessorEditor* MDLFlangerJetAudioProcessor::createEditor()
{
    return new MDLFlangerJetAudioProcessorEditor (*this);
}

void MDLFlangerJetAudioProcessor::ensureStateSize (int numChannels)
{
    if (numChannels <= 0)
        return;

    if ((int) lines.size() < numChannels)
    {
        const auto previous = (int) lines.size();
        lines.resize (numChannels);
        for (int ch = previous; ch < numChannels; ++ch)
            lines[ch].lfoPhase = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
    }

    const auto targetBlock = lastBlockSize > 0 ? lastBlockSize : 512u;
    const bool specChanged = ! juce::approximatelyEqual (delaySpecSampleRate, currentSampleRate)
                             || delaySpecBlockSize != targetBlock;

    if (specChanged)
    {
        juce::dsp::ProcessSpec spec { currentSampleRate, targetBlock, 1 };
        for (auto& line : lines)
        {
            line.delay.prepare (spec);
            line.delay.reset();
        }

        delaySpecSampleRate = currentSampleRate;
        delaySpecBlockSize = targetBlock;
    }
}

void MDLFlangerJetAudioProcessor::updateDelayBounds()
{
    const int maxSamples = (int) juce::jlimit (1.0, (double) currentSampleRate * 0.1, (double) currentSampleRate * 0.1);
    for (auto& line : lines)
        line.delay.setMaximumDelayInSamples (maxSamples);
}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDLFlangerJetAudioProcessor();
}
